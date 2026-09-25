#!/vendor/bin/sh
#
# 讀取 oem 分割區（fstab 唯讀掛載於 /mnt/vendor/oem_cust）的 cust.prop，
# 依 ro.somc.customerid 選出 FeliCa 設定的營運商目錄，結果寫入 vendor.felica.variant。
# 讀不到或不認得的值時不設定屬性，此時不會套用任何營運商設定（Osaifu-Keitai 維持不可用），
# 不會預設成任何一家。

CUST_PROP=/mnt/vendor/oem_cust/system-properties/cust.prop

[ -r "$CUST_PROP" ] || exit 0

customerid=""
while IFS='=' read -r key value; do
    if [ "$key" = "ro.somc.customerid" ]; then
        # 只取開頭的數字，避免行尾殘留字元（例如 CR）
        customerid="${value%%[!0-9]*}"
        break
    fi
done < "$CUST_PROP"

case "$customerid" in
    608)  variant=docomo;;
    8981) variant=softbank;;
    6565) variant=kddi;;
    *)    exit 0;;
esac

setprop vendor.felica.variant "$variant"
