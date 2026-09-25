/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

// Sony NFC HAL（android.hardware.nfc@1.2-service-cxd22xx）的 shim，由 extract-files.py 的
// blob_fixup 加進該執行檔的 DT_NEEDED 第一順位，因此能蓋掉 android.hardware.nfc@1.2.so 的符號。
// （不能用 LD_PRELOAD：init 啟動的 service 一律帶 AT_SECURE，bionic 會忽略它。）
//
// CXD224x 的 FeliCa eSE（T3T 介面的 NFCEE）開 NFC 時一律是停用狀態，而 AOSP NFC stack
// 只會自動啟用 HCI 介面的 NFCEE（UICC），沒有任何路徑會啟用 eSE。這裡攔截 HAL 的
// registerAsService()，改為註冊一層包裝：在 stack 送出「NFCEE 探索關」之前，先對探索時
// 回報為停用的 T3T NFCEE 送 NFCEE_MODE_SET(啟用)，吞掉它的 RSP，並把 NFCC 隨後送出的
// NFCEE_DISCOVER_NTF 交給 stack，讓 NFA 把 eSE 記為 active，後續 routing 就會把 NFC-F 導向 eSE。

#define LOG_TAG "NfcHalShim"

#include <android/hardware/nfc/1.2/INfc.h>
#include <android/hardware/nfc/1.1/INfcClientCallback.h>
#include <dlfcn.h>
#include <log/log.h>

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace {

using android::sp;
using android::status_t;
using android::hardware::hidl_death_recipient;
using android::hardware::hidl_vec;
using android::hardware::Return;
using android::hardware::Void;
using android::hidl::base::V1_0::IBase;
using NfcData = hidl_vec<uint8_t>;
using android::hardware::nfc::V1_0::NfcEvent;
using android::hardware::nfc::V1_0::NfcStatus;
namespace V1_0 = android::hardware::nfc::V1_0;
namespace V1_1 = android::hardware::nfc::V1_1;
namespace V1_2 = android::hardware::nfc::V1_2;

constexpr uint8_t kMtCmd = 1;
constexpr uint8_t kMtRsp = 2;
constexpr uint8_t kMtNtf = 3;
constexpr uint8_t kGidNfcee = 2;
constexpr uint8_t kOidNfceeDiscover = 0;
constexpr uint8_t kOidNfceeModeSet = 1;
constexpr uint8_t kNfceeStatusDisabled = 1;
constexpr uint8_t kNfceeProtocolT3t = 2;

inline uint8_t mt(const NfcData& d) { return (d[0] >> 5) & 0x07; }
inline uint8_t gid(const NfcData& d) { return d[0] & 0x0f; }
inline uint8_t oid(const NfcData& d) { return d[1] & 0x3f; }

// 注入 NFCEE_MODE_SET 時，write() 端與 HAL 回呼端共用的狀態。
class EseActivator {
  public:
    // HAL 回呼端：記下探索時回報為停用的 T3T NFCEE，並攔截注入指令的 RSP。
    // 回傳 true 表示此封包是注入指令的回應，不應交給 stack。
    bool onReceive(const NfcData& d) {
        if (d.size() < 3 || gid(d) != kGidNfcee) return false;
        std::lock_guard<std::mutex> lock(mMutex);
        if (mt(d) == kMtRsp && oid(d) == kOidNfceeModeSet && mInjecting) {
            mRspStatus = d.size() > 3 ? d[3] : 0xff;
            mGotRsp = true;
            mCv.notify_all();
            return true;
        }
        // NFCEE_DISCOVER_NTF：[hdr x3][NFCEE ID][狀態][協定數][協定...]
        if (mt(d) == kMtNtf && oid(d) == kOidNfceeDiscover && d.size() >= 6) {
            uint8_t id = d[3], status = d[4], count = d[5];
            if (mInjecting && id == mInjectId && status != kNfceeStatusDisabled) {
                mGotNtf = true;
                mCv.notify_all();
            } else if (status == kNfceeStatusDisabled) {
                for (size_t i = 0; i < count && 6 + i < d.size(); i++) {
                    if (d[6 + i] == kNfceeProtocolT3t) {
                        mPendingId = id;
                        break;
                    }
                }
            }
        }
        return false;
    }

    // stack 寫入端：在「NFCEE 探索關」送出前啟用待啟用的 NFCEE。
    void beforeWrite(const NfcData& d, const sp<V1_0::INfc>& hal) {
        if (d.size() < 4 || mt(d) != kMtCmd || gid(d) != kGidNfcee ||
            oid(d) != kOidNfceeDiscover || d[3] != 0) {
            return;
        }
        std::unique_lock<std::mutex> lock(mMutex);
        if (mPendingId == 0) return;
        mInjectId = mPendingId;
        mPendingId = 0;
        mInjecting = true;
        mGotRsp = mGotNtf = false;
        lock.unlock();

        const uint8_t cmd[] = {0x20 | kGidNfcee, kOidNfceeModeSet, 0x02, mInjectId, 0x01};
        NfcData data;
        data.setToExternal(const_cast<uint8_t*>(cmd), sizeof(cmd));
        (void)hal->write(data).isOk();

        lock.lock();
        bool rsp = mCv.wait_for(lock, std::chrono::milliseconds(1000), [this] { return mGotRsp; });
        bool ntf = rsp && mRspStatus == 0 &&
                   mCv.wait_for(lock, std::chrono::milliseconds(500), [this] { return mGotNtf; });
        ALOGI("NFCEE 0x%02x MODE_SET enable: rsp=%s status=0x%02x ntf=%s", mInjectId,
              rsp ? "yes" : "timeout", mRspStatus, ntf ? "yes" : "no");
        mInjecting = false;
    }

    // NFC 關閉或 HAL 重新開啟時清掉殘留狀態。
    void reset() {
        std::lock_guard<std::mutex> lock(mMutex);
        mPendingId = 0;
    }

  private:
    std::mutex mMutex;
    std::condition_variable mCv;
    uint8_t mPendingId = 0;
    uint8_t mInjectId = 0;
    bool mInjecting = false;
    bool mGotRsp = false;
    bool mGotNtf = false;
    uint8_t mRspStatus = 0xff;
};

// 包住 stack 傳進來的回呼，讓 shim 先看過 HAL 往上送的每個封包。
class ClientCallbackShim : public V1_1::INfcClientCallback {
  public:
    ClientCallbackShim(const sp<V1_0::INfcClientCallback>& cb, EseActivator& activator)
        : mCb(cb), mCb11(V1_1::INfcClientCallback::castFrom(cb)), mActivator(activator) {}

    Return<void> sendEvent_1_1(V1_1::NfcEvent event, NfcStatus status) override {
        if (mCb11 != nullptr) return mCb11->sendEvent_1_1(event, status);
        return mCb->sendEvent(static_cast<NfcEvent>(event), status);
    }

    Return<void> sendEvent(NfcEvent event, NfcStatus status) override {
        return mCb->sendEvent(event, status);
    }

    Return<void> sendData(const NfcData& data) override {
        if (mActivator.onReceive(data)) return Void();
        return mCb->sendData(data);
    }

    const sp<V1_0::INfcClientCallback>& client() const { return mCb; }

  private:
    sp<V1_0::INfcClientCallback> mCb;
    sp<V1_1::INfcClientCallback> mCb11;
    EseActivator& mActivator;
};

// 對外註冊的 INfc。除了 open/write 之外全部原樣轉給 Sony 的實作。
class NfcShim : public V1_2::INfc, public hidl_death_recipient {
  public:
    explicit NfcShim(const sp<V1_2::INfc>& hal) : mHal(hal) {}

    Return<NfcStatus> open(const sp<V1_0::INfcClientCallback>& cb) override {
        return mHal->open(wrap(cb));
    }

    Return<NfcStatus> open_1_1(const sp<V1_1::INfcClientCallback>& cb) override {
        return mHal->open_1_1(wrap(cb));
    }

    Return<uint32_t> write(const NfcData& data) override {
        mActivator.beforeWrite(data, mHal);
        return mHal->write(data);
    }

    Return<NfcStatus> coreInitialized(const NfcData& data) override {
        return mHal->coreInitialized(data);
    }
    Return<NfcStatus> prediscover() override { return mHal->prediscover(); }
    Return<NfcStatus> close() override { return mHal->close(); }
    Return<NfcStatus> controlGranted() override { return mHal->controlGranted(); }
    Return<NfcStatus> powerCycle() override { return mHal->powerCycle(); }
    Return<void> factoryReset() override { return mHal->factoryReset(); }
    Return<NfcStatus> closeForPowerOffCase() override { return mHal->closeForPowerOffCase(); }
    Return<void> getConfig(getConfig_cb cb) override { return mHal->getConfig(cb); }
    Return<void> getConfig_1_2(getConfig_1_2_cb cb) override { return mHal->getConfig_1_2(cb); }

    // Sony 的實作對 shim 這個本地物件 linkToDeath 不會生效，改由 shim 代為監看 stack 的回呼，
    // stack 行程死掉時照原 HAL 的做法關閉 NFCC。
    void serviceDied(uint64_t, const android::wp<IBase>&) override {
        ALOGW("NFC client died, closing HAL");
        (void)mHal->close().isOk();
    }

  private:
    sp<V1_1::INfcClientCallback> wrap(const sp<V1_0::INfcClientCallback>& cb) {
        mActivator.reset();
        if (mCallback != nullptr) (void)mCallback->client()->unlinkToDeath(this).isOk();
        mCallback = new ClientCallbackShim(cb, mActivator);
        (void)cb->linkToDeath(this, 0).isOk();
        return mCallback;
    }

    sp<V1_2::INfc> mHal;
    sp<ClientCallbackShim> mCallback;
    EseActivator mActivator;
};

sp<NfcShim> gShim;

}  // namespace

// 蓋掉 android.hardware.nfc@1.2.so 的 INfc::registerAsService()：Sony 的 service main 呼叫它時，
// 改用包裝後的物件去註冊。
status_t android::hardware::nfc::V1_2::INfc::registerAsService(const std::string& serviceName) {
    using RegisterFn = status_t (*)(V1_2::INfc*, const std::string&);
    static const auto real = reinterpret_cast<RegisterFn>(dlsym(
            RTLD_NEXT,
            "_ZN7android8hardware3nfc4V1_24INfc17registerAsServiceERKNSt3__112basic_stringIcNS4_"
            "11char_traitsIcEENS4_9allocatorIcEEEE"));
    if (real == nullptr) {
        ALOGE("registerAsService not found: %s", dlerror());
        return android::NAME_NOT_FOUND;
    }
    if (gShim == nullptr) gShim = new NfcShim(this);
    ALOGI("registering NFC HAL shim as %s", serviceName.c_str());
    return real(gShim.get(), serviceName);
}
