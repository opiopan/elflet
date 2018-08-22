#include <esp_log.h>
#include <esp_system.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <GeneralUtils.h>
#include <json11.hpp>
#include "SmartPtr.h"
#include "irrc.h"
#include "Config.h"
#include "ShadowDevice.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "ShadowDevice";

static const char JSON_SHADOW_NAME[] = "ShadowName";
static const char JSON_SHADOW_COMMON_COND[] = "CommonCondition";
static const char JSON_SHADOW_ON_COND[] = "OnCondition";
static const char JSON_SHADOW_OFF_COND[] = "OffCondition";
static const char JSON_SHADOW_ISON[] = "IsOn";
static const char JSON_FORMULA_TYPE[] = "Type";
static const char JSON_FORMULA_OPERATOR[] = "Operator";
static const char JSON_FORMULA_CHILDREN[] = "Formulas";
static const char JSON_FORMULA_PROTOCOL[] = "Protocol";
static const char JSON_FORMULA_BITS[] = "BitCount";
static const char JSON_FORMULA_OFFSET[] = "Offset";
static const char JSON_FORMULA_DATA[] = "Data";
static const char JSON_FORMULA_MASK[] = "Mask";

static const char* FORMULA_TYPE_STR[] = {
    "COMBINATION", "PROTOCOL", "DATA", "ATTRIBUTE", NULL
};
static const char* OP_TYPE_STR[] = {"AND", "OR", "NAND", "NOR", NULL};
static const char* PROTOCOL_STR[] = {"NEC", "AEHA", "SONY", NULL};

//======================================================================
// JSON handling functions
//======================================================================
static bool ApplyValue(const json11::Json& json, const std::string& key,
		       bool notFound,
		       std::function<bool(const std::string&)> apply){
    auto obj = json[key];
    if (obj.is_string()){
	return apply(obj.string_value());
    }
    return notFound;
}

static bool ApplyValue(const json11::Json& json, const std::string& key,
		       bool notFound,
		       std::function<bool(int32_t)> apply){
    auto obj = json[key];
    if (obj.is_number()){
	return apply(obj.int_value());
    }
    return notFound;
}

static bool ApplyHexValue(const json11::Json& json, const std::string& key,
			  bool notFound,
			  std::function<bool(std::string&)> apply){
    auto obj = json[key];
    auto hexval = [](int c)->int {
	if (c >= '0' && c <= '9'){
	    return c - '0';
	}else if (c >= 'a' && c <= 'f'){
	    return c - 'a' + 10;
	}else if (c >= 'A' && c <= 'F'){
	    return c - 'A' + 10;
	}else{
	    return -1;
	}
    };
    if (obj.is_string() && !(obj.string_value().length() & 1)){
	auto in = obj.string_value();
	std::string hexdata;

	for (int i = 0; i < in.length(); i += 2){
	    auto hval = hexval(in[i]);
	    auto lval = hexval(in[i + 1]);
	    if (hval < 0 || lval < 0){
		return false;
	    }
	    hexdata.push_back((hval << 4) | lval);
	}
	
	return apply(hexdata);
    }
    return notFound;
}

//======================================================================
// element of condition formula
//======================================================================

//----------------------------------------------------------------------
// base class
//----------------------------------------------------------------------
template <typename T>  class FormulaNode {
public:
    enum Type {COMBINATION, PROTOCOL, DATA, ATTRIBUTE};
    using Ptr = SmartPtr<FormulaNode<T>>;

protected:
    const Type type;

public:
    FormulaNode(Type type):type(type){};
    virtual ~FormulaNode(){};
    
    Type getType() const {return type;};

    virtual bool deserialize(const json11::Json& in, std::string& err) = 0;
    virtual void serialize(std::ostream& out){
	out << "\"" << JSON_FORMULA_TYPE << "\":\""
	    << FORMULA_TYPE_STR[(int)type] << "\"";
    };
    virtual bool test(const T* testee) = 0;
};


static void createFormula(const json11::Json& in, std::string& err,
			  FormulaNode<IRCommand>::Ptr& ptr);

class ShadowDeviceImp;
static void createFormula(const json11::Json& in, std::string& err,
			  FormulaNode<ShadowDeviceImp>::Ptr& ptr);

//----------------------------------------------------------------------
// combination node
//----------------------------------------------------------------------
template <typename T> class CombinationNode : public FormulaNode<T> {
protected:
    using Base = FormulaNode<T>;
    enum Operation {OP_AND = 0, OP_OR, OP_NAND, OP_NOR};
    Operation operation;
    std::vector<typename Base::Ptr> children;
    
public:
    CombinationNode() : Base(Base::COMBINATION){};
    virtual ~CombinationNode(){};
    
    bool deserialize(const json11::Json& in, std::string& err) override;
    void serialize(std::ostream& out) override;
    bool test(const T* testee) override;
};

template <typename T>
bool CombinationNode<T>::deserialize(const json11::Json& in, std::string& err){
    if (!ApplyValue(in, JSON_FORMULA_OPERATOR, false,
		    [&](const std::string& v) -> bool{
			for (int i = 0; OP_TYPE_STR[i]; i++){
			    if (v == OP_TYPE_STR[i]){
				this->operation = (Operation)i;
				return true;
			    }
			}
			return false;
		    })){
	err = "Invalid combination operator or no operator was specivied.";
	return false;
    }
    auto formulas = in[JSON_FORMULA_CHILDREN];
    if (formulas.is_array()){
	for (auto &f : formulas.array_items()){
	    if (!f.is_object()){
		err = "Sub-formula of combination operator must be "
		      "specified as JSON object.";
		return false;
	    }
	    typename Base::Ptr child;
	    createFormula(json11::Json(f.object_items()), err, child);
	    if (child.isNull()){
		return false;
	    }
	    children.push_back(child);
	}
    }
    if (children.size() == 0){
	err = "At least one sub-formula must be specified "
	      "for combination operator.";
	return false;
    }

    return true;
}

template <typename T>
void CombinationNode<T>::serialize(std::ostream& out){
    out << "{";
    Base::serialize(out);
    out << ",\"" << JSON_FORMULA_OPERATOR << "\":\""
	<< OP_TYPE_STR[(int)operation];
    out << "\",\"" << JSON_FORMULA_CHILDREN << "\":[";
    for (auto i = children.begin(); i != children.end(); i++){
	if (i != children.begin()){
	    out << ",";
	}
	(*i)->serialize(out);
    }
    out << "]}";
}

template <typename T>
bool CombinationNode<T>::test(const T* testee){
    static const uint8_t INITIAL =   0b1000000;
    static const uint8_t BREAKABLE = 0b0100000;
    static const uint8_t BREAK =     0b0010000;
    static const uint8_t TFTABLES[] = {
	0b1000 | (INITIAL & 0xff) | (BREAKABLE & 0xff) | (BREAK & 0x00),
	0b1110 | (INITIAL & 0x00) | (BREAKABLE & 0xff) | (BREAK & 0xff),
	0b0111 | (INITIAL & 0xff) | (BREAKABLE & 0x00) | (BREAK & 0x00),
	0b0001 | (INITIAL & 0x00) | (BREAKABLE & 0x00) | (BREAK & 0x00),
    };
    const auto test = [&](bool l, bool r) -> bool{
	auto bit = 1 << ((l ? 0b10 : 0) | (r ? 0b1 : 0));
	return TFTABLES[(int)this->operation] & bit;
    };

    auto tftable = TFTABLES[(int)operation];
    auto rc = tftable & INITIAL;

    for (auto i = children.begin(); i != children.end(); i++){
	auto rvalue = (*i)->test(testee);
	rc = test(rc, rvalue);
	if (tftable & BREAKABLE &&
	    (rc != 0) == ((tftable & BREAK) != 0)){
	    break;
	}
    }

    ESP_LOGD(tag, "COMBINATION: %d", rc);
    return rc;
}

//----------------------------------------------------------------------
// protocol comparison node
//----------------------------------------------------------------------
class ProtocolNode : public FormulaNode<IRCommand> {
protected:
    using Base = FormulaNode<IRCommand>;

    IRRC_PROTOCOL protocol;
    int32_t bits;
    
public:
    ProtocolNode() : Base(Base::PROTOCOL),
		     protocol(IRRC_UNKNOWN), bits(-1){};
    virtual ~ProtocolNode(){};
    
    bool deserialize(const json11::Json& in, std::string& err) override;
    void serialize(std::ostream& out) override;    
    bool test(const IRCommand* cmd) override;
};

bool ProtocolNode::deserialize(const json11::Json& in, std::string& err){
    if (!ApplyValue(in, JSON_FORMULA_PROTOCOL, false,
		    [&](const std::string& v) -> bool{
			for (int i = 0; PROTOCOL_STR[i]; i++){
			    if (v == PROTOCOL_STR[i]){
				this->protocol = (IRRC_PROTOCOL)i;
				return true;
			    }
			}
			return false;
		    })){
	err = "Invalid protocol type or no protocol type was specivied.";
	return false;
    }
    ApplyValue(in, JSON_FORMULA_BITS, true, [&](int32_t v) -> bool{
	    this->bits = v;
	    return true;
	});
    return true;
}

void ProtocolNode::serialize(std::ostream& out){
    out << "{";
    FormulaNode::serialize(out);
    out << ", \"" << JSON_FORMULA_PROTOCOL << "\":\""
	<< PROTOCOL_STR[(int)protocol] << "\"";
    if (bits > 0){
	out << ", \"" << JSON_FORMULA_BITS << "\":" << bits;
    }
    out << "}";
}

bool ProtocolNode::test(const IRCommand* cmd){
    if (cmd->protocol != protocol){
	ESP_LOGD(tag, "PROTOCOL: false");
	return false;
    }
    if (bits > 0 && bits != cmd->bits){
	ESP_LOGD(tag, "PROTOCOL: false");
	return false;
    }
    ESP_LOGD(tag, "PROTOCOL: true");
    return true;
}

//----------------------------------------------------------------------
// data comparison node
//----------------------------------------------------------------------
class DataNode : public FormulaNode<IRCommand> {
protected:
    using Base = FormulaNode<IRCommand>;

    int32_t offset;
    std::string data;
    std::string mask;
    
public:
    DataNode() : Base(Base::DATA), offset(0){};
    virtual ~DataNode(){};
    
    bool deserialize(const json11::Json& in, std::string& err) override;
    void serialize(std::ostream& out) override;
    bool test(const IRCommand* cmd) override;
};

bool DataNode::deserialize(const json11::Json& in, std::string& err){
    ApplyValue(in, JSON_FORMULA_OFFSET, true, [&](int32_t v) -> bool{
	    this->offset = (v < 0 ? 0 : v);
	    return true;
	});
    if (!ApplyHexValue(in, JSON_FORMULA_DATA, false, [&](std::string& v)->bool{
		this->data = std::move(v);
		return true;
	    })){
	err = "Invalid hex data or no 'Data' attribute was specified.";
	return false;
    }
    if (!ApplyHexValue(in, JSON_FORMULA_MASK, true, [&](std::string& v)->bool{
		this->mask = std::move(v);
		return true;
	    })){
	err = "Invalid hex data for 'Mask' attribute was specified.";
	return false;
    }else if (mask.length() > 0 && data.length() != mask.length()){
	err = "Mask length must be same as data length";
	return false;
    }
	
    return true;
}

void DataNode::serialize(std::ostream& out){
    auto printhex = [](std::ostream& out, const std::string& data){
	for (auto i = data.begin(); i != data.end(); i++){
	    static const char dict[] = "0123456789abcdef";
	    out.put(dict[(*i & 0xf0) >> 4]);
	    out.put(dict[*i & 0xf]);
	}
    };
    
    out << "{";
    FormulaNode::serialize(out);
    if (offset >= 0){
	out << ", \"" << JSON_FORMULA_OFFSET << "\":" << offset;;
    }
    out << ", \"" << JSON_FORMULA_DATA << "\":\"";
    printhex(out, data);
    out << "\"";
    if (mask.length() > 0){
	out << ", \"" << JSON_FORMULA_MASK << "\":\"";
	printhex(out, mask);
	out << "\"";
    }
    out << "}";
}

bool DataNode::test(const IRCommand* cmd){
    if ((cmd->bits + 7) * 8 < data.length() + offset){
	ESP_LOGD(tag, "DATA: false");
	return false;
    }
    if (mask.length() > 0){
	for (int i = 0; i < data.length(); i++){
	    if (data.at(i) !=
		(((const char*)cmd->data)[i + offset] & mask.at(i))){
		ESP_LOGD(tag, "DATA: false");
		return false;
	    }
	}
    }else{
	bool rc =  memcmp((const char*)cmd->data + offset,
			  data.data(), data.length()) == 0;
	
	ESP_LOGD(tag, "DATA: %d", rc);
	return rc;
    }
    ESP_LOGD(tag, "DATA: true");
    return true;
}

//----------------------------------------------------------------------
// formula node factory
//----------------------------------------------------------------------
static void createFormula(const json11::Json& in, std::string& err,
			  FormulaNode<IRCommand>::Ptr& ptr){
    auto type = in[JSON_FORMULA_TYPE];
    ptr = FormulaNode<IRCommand>::Ptr(NULL);
    
    if (type.is_string()){
	auto typeStr = type.string_value();
	if (typeStr == "COMBINATION"){
	    ptr = FormulaNode<IRCommand>::Ptr(new CombinationNode<IRCommand>);
	}else if (typeStr == "PROTOCOL"){
	    ptr = FormulaNode<IRCommand>::Ptr(new ProtocolNode);
	}else if (typeStr == "DATA"){
	    ptr = FormulaNode<IRCommand>::Ptr(new DataNode);
	}else{
	    err = "Invalid formula type was specified.";
	}
	if (!ptr->deserialize(in, err)){
	    ptr = FormulaNode<IRCommand>::Ptr(NULL);
	}
    }else{
	err = "No formula type was specified.";
    }
}

//======================================================================
// shadow device inmplementation
//======================================================================
class ShadowDeviceImp : public ShadowDevice {
protected:
    std::string name;
    FormulaNode<IRCommand>::Ptr commonCondition;
    FormulaNode<IRCommand>::Ptr onCondition;
    FormulaNode<IRCommand>::Ptr offCondition;

    bool powerStatus;

public:
    ShadowDeviceImp(const char* name):name(name), powerStatus(false){};
    virtual ~ShadowDeviceImp(){};

    const std::string& getName() const{return name;};

    bool deserialize(const json11::Json& in, std::string& err);
    void serialize(std::ostream& out) override;
    bool applyIRCommand(const IRCommand* cmd);
        
    bool isOn() override;
    void setPowerStatus(bool isOn) override;
    void dumpStatus(std::ostream& out) override;
};

using ShadowDevicePtr = SmartPtr<ShadowDeviceImp>;

bool ShadowDeviceImp::deserialize(const json11::Json& in, std::string& err){
    auto nameStr = in[JSON_SHADOW_NAME];
    if (nameStr.is_string() && nameStr.string_value() != name){
	err = "Shadow name specified in uri is not same name in JSON data.";
	return false;
    }

    auto applyFormula =
	[&](const char* tag, FormulaNode<IRCommand>::Ptr& ptr)->bool{
	auto defs = in[tag];
	if (defs.is_object()){
	    createFormula(json11::Json(defs.object_items()), err, ptr);
	    if (ptr.isNull()){
		return false;
	    }
	}
	return true;
    };

    if (!applyFormula(JSON_SHADOW_COMMON_COND, commonCondition)){
	return false;
    }
    if (!applyFormula(JSON_SHADOW_ON_COND, onCondition)){
	return false;
    }
    if (!applyFormula(JSON_SHADOW_OFF_COND, offCondition)){
	return false;
    }

    if (onCondition.isNull() && offCondition.isNull()){
	err = "At least eather condition of on or off must be specified.";
	return false;
    }

    if (commonCondition.isNull() &&
	(onCondition.isNull() || offCondition.isNull())){
	err = "Common condition must be specified "
	      "if you omit to specify on condition or off condition.";
	return false;
    }
    
    return true;
}

void ShadowDeviceImp::serialize(std::ostream& out){
    out << "{\"" << JSON_SHADOW_NAME << "\":\"" << name << "\"";
    if (!commonCondition.isNull()){
	out << ",\"" << JSON_SHADOW_COMMON_COND << "\":";
	commonCondition->serialize(out);
    }
    if (!onCondition.isNull()){
	out << ",\"" << JSON_SHADOW_ON_COND << "\":";
	onCondition->serialize(out);
    }
    if (!offCondition.isNull()){
	out << ",\"" << JSON_SHADOW_OFF_COND << "\":";
	offCondition->serialize(out);
    }
    out << "}";
}

bool ShadowDeviceImp::applyIRCommand(const IRCommand* cmd){
    ESP_LOGD(tag, "start commonCondition");
    if (!commonCondition.isNull() &&
	!commonCondition->test(cmd)){
	ESP_LOGD(tag, "commonCondition: false");
	return false;
    }
    ESP_LOGD(tag, "start onCondition");
    if (!onCondition.isNull()){
	if (onCondition->test(cmd)){
	    powerStatus = true;
	    ESP_LOGD(tag, "onCondition: true: on");
	    return true;
	}else if (offCondition.isNull()){
	    ESP_LOGD(tag, "onCondition: true: off");
	    powerStatus = false;
	    return true;
	}
    }
    ESP_LOGD(tag, "start endCondition");
    if (!offCondition.isNull()){
	if (offCondition->test(cmd)){
	    ESP_LOGD(tag, "offCondition: true: off");
	    powerStatus = false;
	    return true;
	}else if (onCondition.isNull()){
	    ESP_LOGD(tag, "offCondition: true: on");
	    powerStatus = true;
	    return true;
	}
    }
    ESP_LOGD(tag, "finary: false");
    return false;
}

bool ShadowDeviceImp::isOn(){
    return powerStatus;
}

void ShadowDeviceImp::setPowerStatus(bool isOn){
    powerStatus = isOn;
}

void ShadowDeviceImp::dumpStatus(std::ostream& out){
    out << "{\"" << JSON_NODENAME << "\":\""
	<< elfletConfig->getNodeName() << "\",\"";
    out << JSON_SHADOW_NAME << "\":\"" << name << "\",\"";
    out << JSON_SHADOW_ISON << "\":"
	<< (isOn() ? "true" : "false") << "}";
}

//======================================================================
// Shadow device list management
//======================================================================
static std::vector<ShadowDevicePtr> shadows;

static bool loadShadowDevicePool(){
    shadows.clear();
    
    auto path = elfletConfig->getShadowDefsPath();
    struct stat sbuf;
    int rc = stat(path, &sbuf);
    if (rc != 0){
	// there is no definition file, it means initial state
	return true;
    }
    auto length = sbuf.st_size;
    char* buf = new char[length];
    FILE* fp = fopen(path, "r");
    rc = fread(buf, length, 1, fp);
    if (rc != 1){
	ESP_LOGE(tag, "cannot read shadow definition file: %s", path);
	fclose(fp);
	delete buf;
	return false;
    }
    fclose(fp);
    std::string contents(buf, length);
    delete buf;

    std::string err;
    auto defs = json11::Json::parse(contents, err);
    if (!defs.is_array()){
	ESP_LOGE(tag, "shadow definition file might corrupted: %s", path);
	return false;
    }

    for (auto &element : defs.array_items()){
	if (element.is_object()){
	    auto object = json11::Json(element.object_items());
	    auto name = object[JSON_SHADOW_NAME].string_value();
	    if (name.length() > 0){
		ShadowDevicePtr shadow(new ShadowDeviceImp(name.c_str()));
		std::string err;
		if (shadow->deserialize(object, err)){
		    shadows.push_back(shadow);
		}else{
		    ESP_LOGE(tag, "shadow devinition might corrupted: %s",
			     name.c_str());
		}
	    }
	}else{
	    ESP_LOGE(tag, "shadow definition might corrupted: %s",
		     element.dump().c_str());
	}
    }

    ESP_LOGI(tag, "%d shadow definitions has been loaded", shadows.size());
    
    return true;
}

static bool saveShadowDevicePool(){
    auto defspath = elfletConfig->getShadowDefsPath();

    auto out = std::ofstream(defspath);
    out << "[";
    for (auto i = shadows.begin(); i != shadows.end(); i++){
	if (i != shadows.begin()){
	    out << ",";
	}
	(*i)->serialize(out);
    }
    out << "]";
    out.close();
    
    return true;
}

bool initShadowDevicePool(){
    return loadShadowDevicePool();
}

bool resetShadowDevicePool(){
    if (remove(elfletConfig->getShadowDefsPath()) != 0){
	ESP_LOGE(tag, "fail to remove shadow definition file");
    }
    return false;
}

bool addShadowDevice(
    const std::string& name, const std::string& json, std::string& err){

    std::string jsonErr;
    auto defs = json11::Json::parse(json, jsonErr);
    if (!defs.is_object()){
	err = "invalid request body format: ";
	err += jsonErr;
	return false;
    }

    ShadowDevicePtr shadow(new ShadowDeviceImp(name.c_str()));
    if (!shadow->deserialize(defs, err)){
	return false;
    }

    auto i = shadows.begin();
    for (; i != shadows.end(); i++){
	if ((*i)->getName() == shadow->getName()){
	    break;
	}
    }

    if (i == shadows.end()){
	shadows.push_back(shadow);
    }else{
	*i = shadow;
    }

    saveShadowDevicePool();
    ESP_LOGI(tag, "added shadow: %s", name.c_str());
        
    return true;
}

bool deleteShadowDevice(const std::string& name){
    for (auto i = shadows.begin(); i != shadows.end(); i++){
	if ((*i)->getName() == name){
	    shadows.erase(i);
	    saveShadowDevicePool();
	    ESP_LOGI(tag, "deleted shadow: %s", name.c_str());
	    return true;;
	}
    }
    return false;
}

ShadowDevice* findShadowDevice(const std::string& name){
    
    for (auto i = shadows.begin(); i != shadows.end(); i++){
	if ((*i)->getName() == name){
	    return *i;
	}
    }
    return NULL;
}

bool dumpShadowDeviceNames(std::ostream& out){
    out << '[';
    for (auto i = shadows.begin(); i != shadows.end(); i++){
	if (i != shadows.begin()){
	    out << ',';
	}
	out << "\"" << (*i)->getName() << "\"";
    }
    out << ']';
    return true;
}

bool applyIRCommand(const IRCommand* cmd){
    for (auto i = shadows.begin(); i != shadows.end(); i++){
	if ((*i)->applyIRCommand(cmd)){
	    ESP_LOGI(tag, "shadow '%s' turned %s",
		     (*i)->getName().c_str(), (*i)->isOn() ? "on" : "off");
	    return true;
	}
    }
    return false;
}
