#!/vendor/bin/sh
#
# 依 oem 分割區 cust.prop 的 ro.somc.customerid 設定 vendor.felica.variant；讀不到或不認得就不設定。

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
