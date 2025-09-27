// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "base58.h"
#include "bitcoinrpc.h"
#include "db.h"
#include "interface.h"
#include "net.h"
#include "random.h"
#include "sync.h"
#include "util.h"
#include "wallet.h"

#undef printf

#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXHttpServer.h>

#include <list>
#include <memory>
#include <regex>

#define printf OutputDebugStringF

using namespace json_spirit;

std::unique_ptr<ix::HttpServer> g_server;

static std::string strRPCUserColonPass;

const Object emptyobj;

static inline unsigned short GetDefaultRPCPort()
{
    return GetBoolArg("-testnet", false) ? 18344 : 8344;
}

Object JSONRPCError(int code, const std::string& message)
{
    Object error;
    error.push_back(Pair("code", code));
    error.push_back(Pair("message", message));
    return error;
}

void RPCTypeCheck(const Array& params,
                  const std::list<Value_type>& typesExpected,
                  bool fAllowNull)
{
    unsigned int i = 0;
    for(Value_type t :  typesExpected)
    {
        if (params.size() <= i)
            break;

        const Value& v = params[i];
        if (!((v.type() == t) || (fAllowNull && (v.type() == null_type))))
        {
            std::string err = strprintf("Expected type %s, got %s",
                                   Value_type_name[t], Value_type_name[v.type()]);
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
        i++;
    }
}

void RPCTypeCheck(const Object& o,
                  const std::map<std::string, Value_type>& typesExpected,
                  bool fAllowNull)
{
    for(const auto& t : typesExpected)
    {
        const Value& v = find_value(o, t.first);
        if (!fAllowNull && v.type() == null_type)
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing %s", t.first.c_str()));

        if (!((v.type() == t.second) || (fAllowNull && (v.type() == null_type))))
        {
            std::string err = strprintf("Expected type %s for %s, got %s",
                                   Value_type_name[t.second], t.first.c_str(), Value_type_name[v.type()]);
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
    }
}

int64_t AmountFromValue(const Value& value)
{
    double dAmount = value.get_real();
    if (dAmount <= 0.0 || dAmount > (double) (MAX_MONEY / 100000000))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    int64_t nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}

Value ValueFromAmount(int64_t amount)
{
    return (double)amount / (double)COIN;
}

std::string HexBits(unsigned int nBits)
{
    union {
        int32_t nBits;
        char cBits[4];
    } uBits;
    uBits.nBits = htonl((int32_t)nBits);
    return HexStr(BEGIN(uBits.cBits), END(uBits.cBits));
}


//
// Utilities: convert hex-encoded Values
// (throws error if not hex).
//
uint256 ParseHashV(const Value& v, const std::string& strName)
{
    std::string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex)) // Note: IsHex("") is false
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    uint256 result;
    result.SetHex(strHex);
    return result;
}

uint256 ParseHashO(const Object& o, const std::string& strKey)
{
    return ParseHashV(find_value(o, strKey), strKey);
}

std::vector<unsigned char> ParseHexV(const Value& v, const std::string& strName)
{
    std::string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    return ParseHex(strHex);
}

std::vector<unsigned char> ParseHexO(const Object& o, const std::string& strKey)
{
    return ParseHexV(find_value(o, strKey), strKey);
}


///
/// Note: This interface may still be subject to change.
///

std::string CRPCTable::help(const std::string& strCommand) const
{
    std::string strRet;
    std::set<rpcfn_type> setDone;
    for (const auto & mapCommand : mapCommands)
    {
        const CRPCCommand *pcmd = mapCommand.second;
        std::string strMethod = mapCommand.first;
        // We already filter duplicates, but these deprecated screw up the sort order
        if (strMethod.find("label") != std::string::npos)
            continue;
        if (!strCommand.empty() && strMethod != strCommand)
            continue;
        try
        {
            Array params;
            rpcfn_type pfn = pcmd->actor;
            if (setDone.insert(pfn).second)
                (*pfn)(params, true);
        }
        catch (const std::exception& e)
        {
            // Help text is returned in an exception
            std::string strHelp = std::string(e.what());
            if (strCommand.empty())
                if (strHelp.find('\n') != std::string::npos)
                    strHelp = strHelp.substr(0, strHelp.find('\n'));
            strRet += strHelp + '\n';
        }
    }
    if (strRet.empty())
        strRet = strprintf("help: unknown command: %s\n", strCommand.c_str());
    strRet = strRet.substr(0,strRet.size()-1);
    return strRet;
}

Value help(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "help [command]\n"
            "List commands, or get help for a command.");

    std::string strCommand;
    if (!params.empty())
        strCommand = params[0].get_str();

    return tableRPC.help(strCommand);
}


Value stop(const Array& params, bool fHelp)
{
// Accept the deprecated and ignored 'detach´ boolean argument
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "stop\n"
            "Stop Novacoin server.");
    // Shutdown will take long enough that the response should get back
    StartShutdown();
    return "NovaCoin server stopping";
}



//
// Call Table
//


static const CRPCCommand vRPCCommands[] =
{ //  name                      function                 safemd  unlocked
  //  ------------------------  -----------------------  ------  --------
    { "help",                       &help,                        true,   true },
    { "stop",                       &stop,                        true,   true },
    { "getbestblockhash",           &getbestblockhash,            true,   false },
    { "getblockcount",              &getblockcount,               true,   false },
    { "getconnectioncount",         &getconnectioncount,          true,   false },
    { "getaddrmaninfo",             &getaddrmaninfo,              true,   false },
    { "getpeerinfo",                &getpeerinfo,                 true,   false },
    { "addnode",                    &addnode,                     true,   true  },
    { "getaddednodeinfo",           &getaddednodeinfo,            true,   true  },
    { "getdifficulty",              &getdifficulty,               true,   false },
    { "getinfo",                    &getinfo,                     true,   false },
    { "getsubsidy",                 &getsubsidy,                  true,   false },
    { "getmininginfo",              &getmininginfo,               true,   false },
    { "scaninput",                  &scaninput,                   true,   true },
    { "getnewaddress",              &getnewaddress,               true,   false },
    { "getnettotals",               &getnettotals,                true,   true  },
    { "ntptime",                    &ntptime,                     true,   true  },
    { "getaccountaddress",          &getaccountaddress,           true,   false },
    { "setaccount",                 &setaccount,                  true,   false },
    { "getaccount",                 &getaccount,                  false,  false },
    { "getaddressesbyaccount",      &getaddressesbyaccount,       true,   false },
    { "sendtoaddress",              &sendtoaddress,               false,  false },
    { "mergecoins",                 &mergecoins,                  false,  false },
    { "getreceivedbyaddress",       &getreceivedbyaddress,        false,  false },
    { "getreceivedbyaccount",       &getreceivedbyaccount,        false,  false },
    { "listreceivedbyaddress",      &listreceivedbyaddress,       false,  false },
    { "listreceivedbyaccount",      &listreceivedbyaccount,       false,  false },
    { "backupwallet",               &backupwallet,                true,   false },
    { "keypoolrefill",              &keypoolrefill,               true,   false },
    { "keypoolreset",               &keypoolreset,                true,   false },
    { "walletpassphrase",           &walletpassphrase,            true,   false },
    { "walletpassphrasechange",     &walletpassphrasechange,      false,  false },
    { "walletlock",                 &walletlock,                  true,   false },
    { "encryptwallet",              &encryptwallet,               false,  false },
    { "validateaddress",            &validateaddress,             true,   false },
    { "getbalance",                 &getbalance,                  false,  false },
    { "move",                       &movecmd,                     false,  false },
    { "sendfrom",                   &sendfrom,                    false,  false },
    { "sendmany",                   &sendmany,                    false,  false },
    { "addmultisigaddress",         &addmultisigaddress,          false,  false },
    { "addredeemscript",            &addredeemscript,             false,  false },
    { "getrawmempool",              &getrawmempool,               true,   false },
    { "getblock",                   &getblock,                    false,  false },
    { "getblockbynumber",           &getblockbynumber,            false,  false },
    { "dumpblock",                  &dumpblock,                   false,  false },
    { "dumpblockbynumber",          &dumpblockbynumber,           false,  false },
    { "getblockhash",               &getblockhash,                false,  false },
    { "gettransaction",             &gettransaction,              false,  false },
    { "listtransactions",           &listtransactions,            false,  false },
    { "listaddressgroupings",       &listaddressgroupings,        false,  false },
    { "signmessage",                &signmessage,                 false,  false },
    { "verifymessage",              &verifymessage,               false,  false },
    { "getwork",                    &getwork,                     true,   false },
    { "getworkex",                  &getworkex,                   true,   false },
    { "listaccounts",               &listaccounts,                false,  false },
    { "settxfee",                   &settxfee,                    false,  false },
    { "getblocktemplate",           &getblocktemplate,            true,   false },
    { "submitblock",                &submitblock,                 false,  false },
    { "listsinceblock",             &listsinceblock,              false,  false },
    { "dumpprivkey",                &dumpprivkey,                 false,  false },
    { "dumpwallet",                 &dumpwallet,                  true,   false },
    { "importwallet",               &importwallet,                false,  false },
    { "importprivkey",              &importprivkey,               false,  false },
    { "importaddress",              &importaddress,               false,  true  },
    { "removeaddress",              &removeaddress,               false,  true  },
    { "listunspent",                &listunspent,                 false,  false },
    { "getrawtransaction",          &getrawtransaction,           false,  false },
    { "createrawtransaction",       &createrawtransaction,        false,  false },
    { "decoderawtransaction",       &decoderawtransaction,        false,  false },
    { "createmultisig",             &createmultisig,              false,  false },
    { "decodescript",               &decodescript,                false,  false },
    { "signrawtransaction",         &signrawtransaction,          false,  false },
    { "sendrawtransaction",         &sendrawtransaction,          false,  false },
    { "getcheckpoint",              &getcheckpoint,               true,   false },
    { "reservebalance",             &reservebalance,              false,  true},
    { "checkwallet",                &checkwallet,                 false,  true},
    { "repairwallet",               &repairwallet,                false,  true},
    { "resendwallettransactions",   &resendwallettransactions,    false,  true},
    { "makekeypair",                &makekeypair,                 false,  true},
    { "newmalleablekey",            &newmalleablekey,             false,  false},
    { "adjustmalleablekey",         &adjustmalleablekey,          false,  false},
    { "adjustmalleablepubkey",      &adjustmalleablepubkey,       false,  false},
    { "listmalleableviews",         &listmalleableviews,          false,  false},
    { "dumpmalleablekey",           &dumpmalleablekey,            false,  false},
    { "importmalleablekey",         &importmalleablekey,          true,   false },
    { "sendalert",                  &sendalert,                   false,  false},
};

CRPCTable::CRPCTable()
{
    for (const auto & vRPCCommand : vRPCCommands)
    {
        const CRPCCommand *pcmd;

        pcmd = &vRPCCommand;
        mapCommands[pcmd->name] = pcmd;
    }
}

const CRPCCommand *CRPCTable::operator[](const std::string& name) const
{
    auto it = mapCommands.find(name);
    if (it == mapCommands.end())
        return nullptr;
    return (*it).second;
}

bool HTTPAuthorized(ix::WebSocketHttpHeaders& mapHeaders)
{
    std::string strAuth = mapHeaders["authorization"];
    if (strAuth.substr(0,6) != "Basic ")
        return false;
    std::string strUserPass64 = std::regex_replace(strAuth.substr(6), static_cast<std::regex>("\\s+"), "");
    std::string strUserPass = DecodeBase64(strUserPass64);
    return TimingResistantEqual(strUserPass, strRPCUserColonPass);
}

//
// JSON-RPC protocol.  Bitcoin speaks version 1.0 for maximum compatibility,
// but uses JSON-RPC 1.1/2.0 standards for parts of the 1.0 standard that were
// unspecified (HTTP errors and contents of 'error').
//
// 1.0 spec: http://json-rpc.org/wiki/specification
// 1.2 spec: http://groups.google.com/group/json-rpc/web/json-rpc-over-http
// http://www.codeproject.com/KB/recipes/JSON_Spirit.aspx
//

std::string JSONRPCRequest(const std::string& strMethod, const Array& params, const Value& id)
{
    Object request;
    request.push_back(Pair("method", strMethod));
    request.push_back(Pair("params", params));
    request.push_back(Pair("id", id));
    return write_string(Value(request), false) + "\n";
}

Object JSONRPCReplyObj(const Value& result, const Value& error, const Value& id)
{
    Object reply;
    if (error.type() != null_type)
        reply.push_back(Pair("result", Value::null));
    else
        reply.push_back(Pair("result", result));
    reply.push_back(Pair("error", error));
    reply.push_back(Pair("id", id));
    return reply;
}

std::string JSONRPCReply(const Value& result, const Value& error, const Value& id)
{
    Object reply = JSONRPCReplyObj(result, error, id);
    return write_string(Value(reply), false) + "\n";
}

std::string ErrorReply(const Object& objError, const Value& id)
{
    // Send error reply from json-rpc error object
    int nStatus = HTTP_INTERNAL_SERVER_ERROR;
    int code = find_value(objError, "code").get_int();
    if (code == RPC_INVALID_REQUEST) nStatus = HTTP_BAD_REQUEST;
    else if (code == RPC_METHOD_NOT_FOUND) nStatus = HTTP_NOT_FOUND;
    return JSONRPCReply(Value::null, objError, id);
}

class JSONRequest
{
public:
    Value id;
    std::string strMethod;
    Array params;

    JSONRequest() { id = Value::null; }
    void parse(const Value& valRequest);
};

void JSONRequest::parse(const Value& valRequest)
{
    // Parse request
    if (valRequest.type() != obj_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Invalid Request object");
    const Object& request = valRequest.get_obj();

    // Parse id now so errors from here on will have the id
    id = find_value(request, "id");

    // Parse method
    Value valMethod = find_value(request, "method");
    if (valMethod.type() == null_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Missing method");
    if (valMethod.type() != str_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Method must be a string");
    strMethod = valMethod.get_str();
    if (strMethod != "getwork" && strMethod != "getblocktemplate")
        printf("RPCServer method=%s\n", strMethod.c_str());

    // Parse params
    Value valParams = find_value(request, "params");
    if (valParams.type() == array_type)
        params = valParams.get_array();
    else if (valParams.type() == null_type)
        params = Array();
    else
        throw JSONRPCError(RPC_INVALID_REQUEST, "Params must be an array");
}

static Object JSONRPCExecOne(const Value& req)
{
    Object rpc_result;

    JSONRequest jreq;
    try {
        jreq.parse(req);

        Value result = tableRPC.execute(jreq.strMethod, jreq.params);
        rpc_result = JSONRPCReplyObj(result, Value::null, jreq.id);
    }
    catch (Object& objError)
    {
        rpc_result = JSONRPCReplyObj(Value::null, objError, jreq.id);
    }
    catch (const std::exception& e)
    {
        rpc_result = JSONRPCReplyObj(Value::null,
                                     JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
    }

    return rpc_result;
}

static std::string JSONRPCExecBatch(const Array& vReq)
{
    Array ret;
    for (const auto & reqIdx : vReq)
        ret.push_back(JSONRPCExecOne(reqIdx));

    return write_string(Value(ret), false) + "\n";
}

static CCriticalSection cs_THREAD_RPCHANDLER;

void StartRPCServer()
{
    strRPCUserColonPass = mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"];
    if (mapArgs["-rpcpassword"].empty())
    {
        unsigned char rand_pwd[32];
        GetRandBytes(rand_pwd, 32);
        std::string strWhatAmI = "To use novacoind";
        if (mapArgs.count("-server"))
            strWhatAmI = strprintf(_("To use the %s option"), "\"-server\"");
        else if (mapArgs.count("-daemon"))
            strWhatAmI = strprintf(_("To use the %s option"), "\"-daemon\"");
        uiInterface.ThreadSafeMessageBox(strprintf(
            _("%s, you must set a rpcpassword in the configuration file:\n %s\n"
              "It is recommended you use the following random password:\n"
              "rpcuser=novacoinrpc\n"
              "rpcpassword=%s\n"
              "(you do not need to remember this password)\n"
              "If the file does not exist, create it with owner-readable-only file permissions.\n"),
                strWhatAmI.c_str(),
                GetConfigFile().string().c_str(),
                EncodeBase58(&rand_pwd[0],&rand_pwd[0]+32).c_str()),
            _("Error"), CClientUIInterface::OK | CClientUIInterface::MODAL);
        StartShutdown();
        return;
    }

    std::string host = GetArg("-rpchost", "127.0.0.1");
    int port = GetArg("-rpcport", GetDefaultRPCPort());

    g_server = std::make_unique<ix::HttpServer>(port, host);

    LOCK(cs_THREAD_RPCHANDLER);

    g_server->setOnConnectionCallback([](const ix::HttpRequestPtr& request, const std::shared_ptr<ix::ConnectionState>& connectionState) -> ix::HttpResponsePtr {

        ix::WebSocketHttpHeaders headers;
        headers["Server"] = std::string("novacoin-json-rpc/") + FormatFullVersion();
        headers["WWW-Authenticate"] = R"(Basic realm="jsonrpc")";

        if (!HTTPAuthorized(request->headers))
        {
            printf("ThreadRPCServer incorrect password attempt from %s\n", connectionState->getRemoteIp().c_str());
            connectionState->setTerminated();
            return std::make_shared<ix::HttpResponse>(HTTP_UNAUTHORIZED, "Unauthorized", ix::HttpErrorCode::Ok, headers, "Not authorized");
        }

        if (request->method != "POST") {
            connectionState->setTerminated();
            return std::make_shared<ix::HttpResponse>(HTTP_BAD_REQUEST, "Bad request", ix::HttpErrorCode::Ok, headers, "Bad request");
        }

        JSONRequest jreq;

        try
        {
            // Parse request
            Value valRequest;
            if (!read_string(request->body, valRequest))
                throw JSONRPCError(RPC_PARSE_ERROR, "Parse error"); 

            std::string strReply;

            // singleton request
            if (valRequest.type() == obj_type) {
                jreq.parse(valRequest);

                // Execute request
                Value result = tableRPC.execute(jreq.strMethod, jreq.params);

                // Send reply
                strReply = JSONRPCReply(result, Value::null, jreq.id);

            // array of requests
            } else if (valRequest.type() == array_type)
                // Execute batch of requests
                strReply = JSONRPCExecBatch(valRequest.get_array());
            else
                throw JSONRPCError(RPC_PARSE_ERROR, "Top-level object parse error");

            // Send reply to client
            return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, strReply);
        
        }
        catch(Object& objError)
        {
            return std::make_shared<ix::HttpResponse>(HTTP_INTERNAL_SERVER_ERROR, "Internal Server Error", ix::HttpErrorCode::Ok, headers, ErrorReply(objError, jreq.id));
        }
        catch(const std::exception& e)
        {
            return std::make_shared<ix::HttpResponse>(HTTP_INTERNAL_SERVER_ERROR, "Internal Server Error", ix::HttpErrorCode::Ok, headers, ErrorReply(JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id));
        }
    });

    std::pair<bool, std::string> result = g_server->listen();
    if (!result.first) {
        auto strerr = strprintf(_("An error occurred while setting up the RPC port %u for listening on host %s: %s"), port, host.c_str(), result.second.c_str());
        uiInterface.ThreadSafeMessageBox(strerr, _("Error"), CClientUIInterface::OK | CClientUIInterface::MODAL);
        return StartShutdown();
    }

    // Run listening thread
    g_server->start();

    // We're listening now
    vnThreadsRunning[THREAD_RPCLISTENER]++;
}

void StopRPCServer()
{
    LOCK(cs_THREAD_RPCHANDLER);
    if (g_server) g_server->stop();
    vnThreadsRunning[THREAD_RPCLISTENER]--;
}

 json_spirit::Value CRPCTable::execute(const std::string &strMethod, const json_spirit::Array &params) const
{
    // Find method
    const CRPCCommand *pcmd = tableRPC[strMethod];
    if (!pcmd)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");

    // Observe safe mode
    std::string strWarning = GetWarnings("rpc");
    if (!strWarning.empty() && !GetBoolArg("-disablesafemode") &&
        !pcmd->okSafeMode)
        throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, std::string("Safe mode: ") + strWarning);

    try
    {
        // Execute
        Value result;
        {
            if (pcmd->unlocked)
                result = pcmd->actor(params, false);
            else {
                LOCK2(cs_main, pwalletMain->cs_wallet);
                result = pcmd->actor(params, false);
            }
        }
        return result;
    }
    catch (const std::exception& e)
    {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }
}

std::vector<std::string> CRPCTable::listCommands() const
{
    std::vector<std::string> commandList;
    for (const auto& i : mapCommands) commandList.emplace_back(i.first);
    return commandList;
}

Object CallRPC(const std::string& strMethod, const Array& params)
{
    if (mapArgs["-rpcuser"].empty() && mapArgs["-rpcpassword"].empty())
        throw std::runtime_error(strprintf(
            _("You must set rpcpassword=<password> in the configuration file:\n%s\n"
              "If the file does not exist, create it with owner-readable-only file permissions."),
                GetConfigFile().string().c_str()));

    // Init net subsystem
    ix::initNetSystem();

    // Create HTTP client
    ix::HttpClient httpClient;
    ix::HttpRequestArgsPtr args = httpClient.createRequest();

    // HTTP basic authentication
    ix::WebSocketHttpHeaders mapRequestHeaders;
    std::string strUserPass64 = EncodeBase64(mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"]);
    mapRequestHeaders["Authorization"] = std::string("Basic ") + strUserPass64;
    args->extraHeaders = mapRequestHeaders;

    // Timeouts
    args->connectTimeout = GetArgInt("-rpc_connecttimeout", 30000);
    args->transferTimeout = GetArgInt("-rpc_transfertimeout", 30000);

    bool fUseSSL = GetBoolArg("-rpcssl");
    std::string url = std::string(fUseSSL ? "https://" : "http://") + GetArg("-rpcconnect", "127.0.0.1") + ":" + GetArg("-rpcport", itostr(GetDefaultRPCPort()));

    // Send request
    std::string strRequest = JSONRPCRequest(strMethod, params, GetRandInt(INT32_MAX));
    auto out = httpClient.post(url, strRequest, args);

    // Process reply
    int nStatus = out->statusCode;
    std::string strReply = out->body;
    ix::WebSocketHttpHeaders mapHeaders = out->headers;

    // Receive reply
    if (nStatus == HTTP_UNAUTHORIZED)
        throw std::runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    else if (nStatus >= HTTP_BAD_REQUEST && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR)
        throw std::runtime_error(strprintf("server returned HTTP error %d", nStatus));
    else if (strReply.empty())
        throw std::runtime_error("no response from server");

    // Parse reply
    Value valReply;
    if (!read_string(strReply, valReply))
        throw std::runtime_error("couldn't parse reply from server");
    const Object& reply = valReply.get_obj();
    if (reply.empty())
        throw std::runtime_error("expected reply to have result, error and id properties");

    return reply;
}




template<typename T>
void ConvertTo(Value& value, bool fAllowNull=false)
{
    if (fAllowNull && value.type() == null_type)
        return;
    if (value.type() == str_type)
    {
        // reinterpret string as unquoted json value
        Value value2;
        std::string strJSON = value.get_str();
        if (!read_string(strJSON, value2))
            throw std::runtime_error(std::string("Error parsing JSON:")+strJSON);
        ConvertTo<T>(value2, fAllowNull);
        value = value2;
    }
    else
    {
        value = value.get_value<T>();
    }
}

// Convert strings to command-specific RPC representation
Array RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    Array params;
    for(const auto &param :  strParams)
        params.push_back(param);

    size_t n = params.size();

    //
    // Special case non-string parameter types
    //
    if (strMethod == "stop"                   && n > 0) ConvertTo<bool>(params[0]);
    if (strMethod == "getaddednodeinfo"       && n > 0) ConvertTo<bool>(params[0]);
    if (strMethod == "sendtoaddress"          && n > 1) ConvertTo<double>(params[1]);
    if (strMethod == "mergecoins"            && n > 0) ConvertTo<double>(params[0]);
    if (strMethod == "mergecoins"            && n > 1) ConvertTo<double>(params[1]);
    if (strMethod == "mergecoins"            && n > 2) ConvertTo<double>(params[2]);
    if (strMethod == "settxfee"               && n > 0) ConvertTo<double>(params[0]);
    if (strMethod == "getreceivedbyaddress"   && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "getreceivedbyaccount"   && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "listreceivedbyaddress"  && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "listreceivedbyaddress"  && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "listreceivedbyaccount"  && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "listreceivedbyaccount"  && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "getbalance"             && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "getblock"               && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "getblockbynumber"       && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "dumpblockbynumber"      && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "getblockbynumber"       && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "getblockhash"           && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "move"                   && n > 2) ConvertTo<double>(params[2]);
    if (strMethod == "move"                   && n > 3) ConvertTo<int64_t>(params[3]);
    if (strMethod == "sendfrom"               && n > 2) ConvertTo<double>(params[2]);
    if (strMethod == "sendfrom"               && n > 3) ConvertTo<int64_t>(params[3]);
    if (strMethod == "listtransactions"       && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "listtransactions"       && n > 2) ConvertTo<int64_t>(params[2]);
    if (strMethod == "listaccounts"           && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "walletpassphrase"       && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "walletpassphrase"       && n > 2) ConvertTo<bool>(params[2]);
    if (strMethod == "getblocktemplate"       && n > 0) ConvertTo<Object>(params[0]);
    if (strMethod == "listsinceblock"         && n > 1) ConvertTo<int64_t>(params[1]);

    if (strMethod == "scaninput"              && n > 0) ConvertTo<Object>(params[0]);

    if (strMethod == "sendalert"              && n > 2) ConvertTo<int64_t>(params[2]);
    if (strMethod == "sendalert"              && n > 3) ConvertTo<int64_t>(params[3]);
    if (strMethod == "sendalert"              && n > 4) ConvertTo<int64_t>(params[4]);
    if (strMethod == "sendalert"              && n > 5) ConvertTo<int64_t>(params[5]);
    if (strMethod == "sendalert"              && n > 6) ConvertTo<int64_t>(params[6]);

    if (strMethod == "sendmany"               && n > 1) ConvertTo<Object>(params[1]);
    if (strMethod == "sendmany"               && n > 2) ConvertTo<int64_t>(params[2]);
    if (strMethod == "reservebalance"         && n > 0) ConvertTo<bool>(params[0]);
    if (strMethod == "reservebalance"         && n > 1) ConvertTo<double>(params[1]);
    if (strMethod == "addmultisigaddress"     && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "addmultisigaddress"     && n > 1) ConvertTo<Array>(params[1]);
    if (strMethod == "listunspent"            && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "listunspent"            && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "listunspent"            && n > 2) ConvertTo<Array>(params[2]);
    if (strMethod == "getrawtransaction"      && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "createrawtransaction"   && n > 0) ConvertTo<Array>(params[0]);
    if (strMethod == "createrawtransaction"   && n > 1) ConvertTo<Object>(params[1]);
    if (strMethod == "createmultisig"         && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "createmultisig"         && n > 1) ConvertTo<Array>(params[1]);
    if (strMethod == "signrawtransaction"     && n > 1) ConvertTo<Array>(params[1], true);
    if (strMethod == "signrawtransaction"     && n > 2) ConvertTo<Array>(params[2], true);
    if (strMethod == "keypoolrefill"          && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "keypoolreset"           && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "importaddress"          && n > 2) ConvertTo<bool>(params[2]);
    if (strMethod == "importprivkey"          && n > 2) ConvertTo<bool>(params[2]);

    return params;
}

int CommandLineRPC(int argc, char *argv[])
{
    std::string strPrint;
    int nRet = 0;
    try
    {
        // Skip switches
        while (argc > 1 && IsSwitchChar(argv[1][0]))
        {
            argc--;
            argv++;
        }

        // Method
        if (argc < 2)
            throw std::runtime_error("too few parameters");
        std::string strMethod = argv[1];

        // Parameters default to strings
        std::vector<std::string> strParams(&argv[2], &argv[argc]);
        Array params = RPCConvertValues(strMethod, strParams);

        // Execute
        Object reply = CallRPC(strMethod, params);

        // Parse reply
        const Value& result = find_value(reply, "result");
        const Value& error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            strPrint = "error: " + write_string(error, false);
            int code = find_value(error.get_obj(), "code").get_int();
            nRet = abs(code);
        }
        else
        {
            // Result
            if (result.type() == null_type)
                strPrint.clear();
            else if (result.type() == str_type)
                strPrint = result.get_str();
            else
                strPrint = write_string(result, true);
        }
    }
    catch (const std::exception& e)
    {
        strPrint = std::string("error: ") + e.what();
        nRet = 87;
    }
    catch (...)
    {
        PrintException(nullptr, "CommandLineRPC()");
    }

    if (!strPrint.empty())
    {
        fprintf((nRet == 0 ? stdout : stderr), "%s\n", strPrint.c_str());
    }
    return nRet;
}


const CRPCTable tableRPC;
