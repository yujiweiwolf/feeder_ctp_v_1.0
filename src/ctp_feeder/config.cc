#include <mutex>
#include <thread>

#include <x/x.h>

#include "config.h"
#include "yaml-cpp/yaml.h"


namespace co {

    Config* Config::instance_ = 0;

    Config* Config::Instance() {
        static std::once_flag flag;
        std::call_once(flag, [&]() {
            if (instance_ == 0) {
                instance_ = new Config();
                instance_->Init();
            }
        });
        return instance_;
    }

    void Config::Init() {
        auto getStr = [&](const YAML::Node& node, const std::string& name) {
            try {
                return node[name] && !node[name].IsNull() ? node[name].as<std::string>() : "";
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };
        auto getStrings = [&](std::vector<std::string>* ret, const YAML::Node& node, const std::string& name, bool drop_empty = false) {
            try {
                if (node[name] && !node[name].IsNull()) {
                    for (auto item : node[name]) {
                        std::string s = x::Trim(item.as<std::string>());
                        if (!drop_empty || !s.empty()) {
                            ret->emplace_back(s);
                        }
                    }
                }
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };
        auto getInt = [&](const YAML::Node& node, const std::string& name, const int64_t& default_value = 0) {
            try {
                return node[name] && !node[name].IsNull() ? node[name].as<int64_t>() : default_value;
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };
        auto getBool = [&](const YAML::Node& node, const std::string& name) {
            try {
                return node[name] && !node[name].IsNull() ? node[name].as<bool>() : false;
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };

        auto filename = x::FindFile("feeder.yaml");
        YAML::Node root = YAML::LoadFile(filename);
        opt_ = QOptions::Load(filename);

        auto  feeder = root["ctp"];
        ctp_market_front_ = getStr( feeder, "ctp_market_front");
        ctp_trade_front_ = getStr( feeder, "ctp_trade_front");
        ctp_broker_id_ = getStr( feeder, "ctp_broker_id");
        ctp_investor_id_ = getStr( feeder, "ctp_investor_id");
        ctp_password_ = getStr( feeder, "ctp_password");
        ctp_password_ = DecodePassword(ctp_password_);
        ctp_app_id_ = getStr( feeder, "ctp_app_id");
        ctp_product_info_ = getStr( feeder, "ctp_product_info");
        ctp_auth_code_ = getStr( feeder, "ctp_auth_code");
        stringstream ss;
        ss << "+-------------------- configuration begin --------------------+" << endl;
        ss << opt_->ToString() << endl;
        ss << endl;
        ss << "ctp:" << endl
            << "  ctp_market_front: " << ctp_market_front_ << endl
            << "  ctp_trade_front: " << ctp_trade_front_ << endl
            << "  ctp_broker_id: " << ctp_broker_id_ << endl
            << "  ctp_investor_id: " << ctp_investor_id_ << endl
            << "  ctp_password: " << string(ctp_password_.size(), '*') << endl
            << "  ctp_app_id: " << ctp_app_id_ << endl
            << "  ctp_product_info: " << ctp_product_info_ << endl
            << "  ctp_auth_code: " << ctp_auth_code_ << endl;
        ss << "+-------------------- configuration end   --------------------+";
        __info << endl << ss.str();
    }

}