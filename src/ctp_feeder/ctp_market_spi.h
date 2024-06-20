#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <boost/algorithm/string.hpp>

#include <x/x.h>
#include "coral/coral.h"
#include "feeder/feeder.h"
#include "ctp.h"
#include "ctp_support.h"
#include "define.h"
#include "config.h"

using namespace std;
using namespace x;

namespace co {

    class CTPMarketSpi : public CThostFtdcMdSpi {
    public:
        CTPMarketSpi(CThostFtdcMdApi* qApi);
        virtual ~CTPMarketSpi();

        ///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
        virtual void OnFrontConnected();

        ///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
        ///@param nReason 错误原因
        ///        0x1001 网络读失败
        ///        0x1002 网络写失败
        ///        0x2001 接收心跳超时
        ///        0x2002 发送心跳失败
        ///        0x2003 收到错误报文
        virtual void OnFrontDisconnected(int nReason);

        ///登录请求响应
        virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

        ///订阅行情应答
        virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

        ///请求查询组播合约响应
        // virtual void OnRspQryMulticastInstrument(CThostFtdcMulticastInstrumentField* pMulticastInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

        ///深度行情通知
        virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData);

        ///错误应答
        virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    protected:
        int next_id();
        void ReqUserLogin();
        void ReqSubMarketData();

    private:
        int ctp_request_id_ = 0;
        CThostFtdcMdApi* api_ = nullptr;

        map<string, int> sub_status_; // key为订阅的代码，value订阅状态：0-订阅中，1-成功，-1：失败
        set<string> first_flag_;
        int64_t local_date_ = 0;
        int64_t login_trading_day_ = 0;
    };
}
