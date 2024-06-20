#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <memory>

#include <x/x.h>
#include "coral/coral.h"
#include "ctp.h"
#include "ctp_support.h"
#include "define.h"
#include "config.h"

using namespace std;
using namespace x;

namespace co {

    class CTPTradeSpi : public CThostFtdcTraderSpi {
    public:
        CTPTradeSpi(CThostFtdcTraderApi* api);
        virtual ~CTPTradeSpi();

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

        /// 客户端认证响应
        virtual void OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

        ///登录请求响应
        virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

        ///登出请求响应
        virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

        ///请求查询交易所响应
        //virtual void OnRspQryExchange(CThostFtdcExchangeField *pExchange, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

        ///请求查询合约响应
        virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

        ///错误应答
        virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

        // 等待查询合约信息结束
        void Wait();

    protected:
        int next_id();
        void ReqAuthenticate(); // 客户端认证请求
        void ReqUserLogin();
        void ReqQryInstrument();

    private:
        bool over_ = false; // 查询合约信息是否已结束
        int ctp_request_id_ = 0;
        string cur_req_type_; // 当前请求类型，用于在OnRspError中进行重试。
        int64_t date_ = 0; // 当前交易日，登陆成功后获取
        CThostFtdcTraderApi* api_ = nullptr;

    };
}
