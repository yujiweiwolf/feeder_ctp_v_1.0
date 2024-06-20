#include "ctp_support.h"

namespace co {


    // �����ж�
    bool is_flow_control(int result) {
        return ((result == -2) || (result == -3));
    }

    string ctp_str(const char* data) {
        string str = data;
        try {
            str = x::GBKToUTF8(str);
        } catch (...) {

        }
        return str;
    }

    int64_t ctp_market2std(TThostFtdcExchangeIDType v) {
        /////////////////////////////////////////////////////////////////////////
        ///TFtdcExchangeIDType��һ����������������
        /////////////////////////////////////////////////////////////////////////
        //typedef char TThostFtdcExchangeIDType[9];
        int64_t std_market = 0;
        string s = "." + string(v);
        if (s == kSuffixCFFEX) {
            std_market = kMarketCFFEX;
        } else if (s == kSuffixSHFE) {
            std_market = kMarketSHFE;
        } else if (s == kSuffixDCE) {
            std_market = kMarketDCE;
        } else if (s == kSuffixCZCE) {
            std_market = kMarketCZCE;
        } else if (s == kSuffixINE) {
            std_market = kMarketINE;
        } else if (s == ".GFEX") {
            std_market = kMarketGFE;
        } else {
            __error << "unknown ctp_market: " << v;
        }
        return std_market;
    }

}

