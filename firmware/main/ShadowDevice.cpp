#include <esp_log.h>
#include <esp_system.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <limits>
#include <iomanip>
#include <GeneralUtils.h>
#include <json11.hpp>
#include "SmartPtr.h"
#include "irrc.h"
#include "Config.h"
#include "ShadowDevice.h"
#include "PubSubService.h"
#include "Stat.h"
#include "IRService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "ShadowDevice";

static const char JSON_SHADOW_NAME[] = "ShadowName";
static const char JSON_SHADOW_COMMON_COND[] = "CommonCondition";
static const char JSON_SHADOW_ON_COND[] = "OnCondition";
static const char JSON_SHADOW_OFF_COND[] = "OffCondition";
static const char JSON_SHADOW_ISON[] = "IsOn";
static const char JSON_SHADOW_ATTRIBUTES[] = "Attributes";
static const char JSON_SHADOW_SYN[] = "Synthesizing";
static const char JSON_SHADOW_SYNOFF[] = "SynthesizingToOff";
static const char JSON_FORMULA_TYPE[] = "Type";
static const char JSON_FORMULA_OPERATOR[] = "Operator";
static const char JSON_FORMULA_CHILDREN[] = "Formulas";
static const char JSON_FORMULA_PROTOCOL[] = "Protocol";
static const char JSON_FORMULA_BITS[] = "BitCount";
static const char JSON_FORMULA_OFFSET[] = "Offset";
static const char JSON_FORMULA_DATA[] = "Data";
static const char JSON_FORMULA_MASK[] = "Mask";
static const char JSON_FORMULA_ATTRNAME[] = "AttrName";
static const char JSON_FORMULA_ATTRVALUE[] = "Value";
static const char JSON_ATTR_NAME[] = "AttrName";
static const char JSON_ATTR_NOAPPINOFF[] = "NotApplyInOff";
static const char JSON_ATTR_VALUE[] = "Value";
static const char JSON_ATTR_DICT[] = "Dictionary";
static const char JSON_ATTR_DEFAULT[] = "Default";
static const char JSON_ATTR_MAX[] = "Max";
static const char JSON_ATTR_MIN[] = "Min";
static const char JSON_ATTR_UNIT[] = "Unit";
static const char JSON_ATTR_APPLY_COND[] = "ApplyCondition";
static const char JSON_ATTR_VISIBLE_COND[] = "VisibleCondition";
static const char JSON_ATTRVAL_TYPE[] = "Type";
static const char JSON_ATTRVAL_OFFSET[] = "Offset";
static const char JSON_ATTRVAL_MASK[] = "Mask";
static const char JSON_ATTRVAL_BIAS[] = "Bias";
static const char JSON_ATTRVAL_DIV[] = "DividedBy";
static const char JSON_ATTRVAL_MUL[] = "MultiplyBy";
static const char JSON_ATTRVAL_CHILDREN[] = "Values";
static const char JSON_SYN_PROTOCOL[] = "Protocol";
static const char JSON_SYN_BITS[] = "BitCount";
static const char JSON_SYN_PLACEHOLDER[] = "Placeholder";
static const char JSON_SYN_VARS[] = "Variables";
static const char JSON_SYN_REDUNDANT[] = "Redundant";
static const char JSON_SYNVER_OFFSET[] = "Offset";
static const char JSON_SYNVER_MASK[] = "Mask";
static const char JSON_SYNVER_BIAS[] = "Bias";
static const char JSON_SYNVER_MUL[] = "MultiplyBy";
static const char JSON_SYNVER_DIV[] = "DividedBy";
static const char JSON_SYNVER_RELATTR[] = "RelateWithVisibilityOfAttribute";
static const char JSON_SYNVER_VALUE[] = "Value";
static const char JSON_SYNVAL_TYPE[] = "Type";
static const char JSON_SYNVAL_ATTRNAME[] = "AttrName";
static const char JSON_SYNVAL_CONSTVAL[] = "Value";
static const char JSON_SYNVAL_SUBFORMULA[] = "SubFormula";
static const char JSON_SYNRED_OPERATOR[] = "Operator";
static const char JSON_SYNRED_OFFSET[] = "Offset";
static const char JSON_SYNRED_SEED[] = "Seed";

static const char* FORMULA_TYPE_STR[] = {
    "COMBINATION", "PROTOCOL", "DATA", "ATTRIBUTE", NULL
};
static const char* OP_TYPE_STR[] = {"AND", "OR", "NAND", "NOR", NULL};
static const char* PROTOCOL_STR[] = {"NEC", "AEHA", "SONY", NULL};
static const char* VFORMULA_TYPE_STR[] = {"PORTION", "ADD", NULL};
static const char* SYNVAL_TYPE_STR[] = {
    "INTEGER", "DECIMAL", "POWER", "ATTRIBUTE", "CONSTANT"
};
static const char* SYN_REDUNDANT_TYPE_STR[] = {"CHECKSUM", "4BITSUB", NULL};


//======================================================================
// JSON handling functions
//======================================================================
static std::string strToHex(const std::string& str){
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
    std::string hexdata;
    if (!(str.length() & 1)){
	for (int i = 0; i < str.length(); i += 2){
	    auto hval = hexval(str[i]);
	    auto lval = hexval(str[i + 1]);
	    if (hval < 0 || lval < 0){
		hexdata.clear();
		return std::move(hexdata);
	    }
	    hexdata.push_back((hval << 4) | lval);
	}
    }
    return std::move(hexdata);
}

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

static bool ApplyBoolValue(const json11::Json& json, const std::string& key,
			   bool notFound,
			   std::function<bool(bool)> apply){
    auto obj = json[key];
    if (obj.is_bool()){
	return apply(obj.bool_value());
    }
    return notFound;
}

static bool ApplyNumValue(const json11::Json& json, const std::string& key,
		       bool notFound,
		       std::function<bool(float)> apply){
    auto obj = json[key];
    if (obj.is_number()){
	return apply(obj.number_value());
    }
    return notFound;
}

static bool ApplyHexValue(const json11::Json& json, const std::string& key,
			  bool notFound,
			  std::function<bool(std::string&)> apply){
    auto obj = json[key];
    if (obj.is_string() && !(obj.string_value().length() & 1)){
	auto hex = strToHex(obj.string_value());
	if (hex.length() == 0){
	    return false;
	}
	return apply(hex);
    }
    return notFound;
}

static void printhex(std::ostream& out, const std::string& data){
    for (auto i = data.begin(); i != data.end(); i++){
	static const char dict[] = "0123456789abcdef";
	out.put(dict[(*i & 0xf0) >> 4]);
	out.put(dict[*i & 0xf]);
    }
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

static void createFormula(const json11::Json& in, std::string& err,
			  FormulaNode<ShadowDevice>::Ptr& ptr);

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
    static const uint8_t INITIAL =   0b100000;
    static const uint8_t BREAK =     0b010000;
    static const uint8_t TFTABLES[] = {
	0b1000 | (INITIAL & 0xff) | (BREAK & 0x00), // AND
	0b1110 | (INITIAL & 0x00) | (BREAK & 0xff), // OR
	0b0111 | (INITIAL & 0xff) | (BREAK & 0x00), // NAND
	0b0001 | (INITIAL & 0x00) | (BREAK & 0xff), // NOR
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
	if ((rvalue != 0) == ((tftable & BREAK) != 0)){
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
    out << "{";
    FormulaNode::serialize(out);
    if (offset >= 0){
	out << ",\"" << JSON_FORMULA_OFFSET << "\":" << offset;;
    }
    out << ",\"" << JSON_FORMULA_DATA << "\":\"";
    printhex(out, data);
    out << "\"";
    if (mask.length() > 0){
	out << ",\"" << JSON_FORMULA_MASK << "\":\"";
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
// attribute comparison node
//----------------------------------------------------------------------
class AttributeNode : public FormulaNode<ShadowDevice> {
protected:
    using Base = FormulaNode<ShadowDevice>;

    std::string attrName;
    bool compareAsString;
    float numericValue;
    std::string stringValue;
    
public:
    AttributeNode() : Base(Base::ATTRIBUTE),
		 compareAsString(false), numericValue(0){};
    virtual ~AttributeNode(){};
    
    bool deserialize(const json11::Json& in, std::string& err) override;
    void serialize(std::ostream& out) override;
    bool test(const ShadowDevice* shadow) override;
};

bool AttributeNode::deserialize(const json11::Json& in, std::string& err){
    if (!ApplyValue(in, JSON_FORMULA_ATTRNAME, false, [&](const std::string v){
		this->attrName = v;
		return true;
	    })){
	err = "Attribute name must be specified for Attribute "
	      "comparison node.";
	return false;
    }
    auto value = in[JSON_FORMULA_ATTRVALUE];
    if (value.is_string()){
	compareAsString = true;
	stringValue = value.string_value();
    }else if (value.is_number()){
	compareAsString = false;
	numericValue = value.number_value();
    }else{
	err = "Attribute value type is invalid or "
	    "attribute value is not specified.";
	return false;
    }
    return true;
}

void AttributeNode::serialize(std::ostream& out){
    out << "{";
    FormulaNode::serialize(out);
    out << ",\"" << JSON_FORMULA_ATTRNAME << "\":\"" << attrName << "\",\"";
    out << JSON_FORMULA_ATTRVALUE << "\":";
    if (compareAsString){
	out << "\"" << stringValue << "\"}";
    }else{
	out << numericValue << "}";
    }
}

bool AttributeNode::test(const ShadowDevice* shadow){
    auto value = shadow->getAttribute(attrName);
    if (compareAsString){
	return value->getStringValue() == stringValue;
    }else{
	return value->getNumericValue() == numericValue;
    }
}

//----------------------------------------------------------------------
// formula node factory
//----------------------------------------------------------------------
static void createFormula(const json11::Json& in, std::string& err,
			  FormulaNode<IRCommand>::Ptr& ptr){
    using Formula = FormulaNode<IRCommand>;
    auto type = in[JSON_FORMULA_TYPE];
    ptr = Formula::Ptr(NULL);
    
    if (type.is_string()){
	auto typeStr = type.string_value();
	if (typeStr == FORMULA_TYPE_STR[Formula::COMBINATION]){
	    ptr = Formula::Ptr(new CombinationNode<IRCommand>);
	}else if (typeStr == FORMULA_TYPE_STR[Formula::PROTOCOL]){
	    ptr = Formula::Ptr(new ProtocolNode);
	}else if (typeStr == FORMULA_TYPE_STR[Formula::DATA]){
	    ptr = Formula::Ptr(new DataNode);
	}else{
	    err = "Invalid formula type was specified.";
	}
	if (!ptr->deserialize(in, err)){
	    ptr = Formula::Ptr(NULL);
	}
    }else{
	err = "No formula type was specified.";
    }
}

static void createFormula(const json11::Json& in, std::string& err,
			  FormulaNode<ShadowDevice>::Ptr& ptr){
    using Formula = FormulaNode<ShadowDevice>;
    auto type = in[JSON_FORMULA_TYPE];
    ptr = Formula::Ptr(NULL);
    
    if (type.is_string()){
	auto typeStr = type.string_value();
	if (typeStr == FORMULA_TYPE_STR[Formula::COMBINATION]){
	    ptr = Formula::Ptr(new CombinationNode<ShadowDevice>);
	}else if (typeStr == FORMULA_TYPE_STR[Formula::ATTRIBUTE]){
	    ptr = Formula::Ptr(new AttributeNode);
	}else{
	    err = "Invalid formula type was specified.";
	}
	if (!ptr->deserialize(in, err)){
	    ptr = Formula::Ptr(NULL);
	}
    }else{
	err = "No formula type was specified.";
    }
}

//======================================================================
// attribute value formula nodes
//======================================================================

//----------------------------------------------------------------------
// base class
//----------------------------------------------------------------------
class ValueFormulaNode {
public:
    enum Type {PORTION, ADD};
    using Ptr = SmartPtr<ValueFormulaNode>;

protected:
    const Type type;

public:
    ValueFormulaNode(Type type):type(type){};
    virtual ~ValueFormulaNode(){};
    
    Type getType() const {return type;};

    virtual bool deserialize(const json11::Json& in, std::string& err) = 0;
    virtual void serialize(std::ostream& out){
	out << "\"" << JSON_ATTRVAL_TYPE << "\":\""
	    << VFORMULA_TYPE_STR[type] << "\"";
    };
    virtual float extract(const IRCommand* cmd) = 0;
};

static void createValueFormula(const json11::Json& in, std::string& err,
			       ValueFormulaNode::Ptr& ptr);

//----------------------------------------------------------------------
// portion data node
//----------------------------------------------------------------------
class PortionNode : public ValueFormulaNode{
protected:
    using Base = ValueFormulaNode;
    int offset;
    uint8_t mask;
    float divFactor;
    float mulFactor;
    float bias;

public:
    PortionNode(): Base(Base::PORTION),
		   mask(0xff), divFactor(1.0), mulFactor(1.0), bias(0.0){};
    virtual ~PortionNode(){};
    bool deserialize(const json11::Json& in, std::string& err) override;
    void serialize(std::ostream& out) override;
    float extract(const IRCommand* cmd) override;
};

bool PortionNode::deserialize(const json11::Json& in, std::string& err){
    if (!ApplyValue(in, JSON_ATTRVAL_OFFSET, false, [&](int32_t v){
		this->offset = v;
		return true;
	    })){
	err = "Offset must be specified for portion node.";
	return false;
    }
    if (!ApplyHexValue(in, JSON_ATTRVAL_MASK, true, [&](const std::string& v){
		if (v.length() != 1){
		    return false;
		}
		this->mask = v[0];
		return true;
	    })){
	err = "Maks in portion node  must be 1 byte hex data as string.";
	return false;
    }
    ApplyNumValue(in, JSON_ATTRVAL_BIAS, true, [&](float v){
	    bias = v;
	    return true;
	});
    ApplyNumValue(in, JSON_ATTRVAL_DIV, true, [&](float v){
	    divFactor = v;
	    return true;
	});
    ApplyNumValue(in, JSON_ATTRVAL_MUL, true, [&](float v){
	    mulFactor = v;
	    return true;
	});

    return true;
}

void PortionNode::serialize(std::ostream& out){
    out << "{";
    Base::serialize(out);
    out << ",\"" << JSON_ATTRVAL_OFFSET << "\":" << offset;
    if (mask != 0xff){
	out << ",\"" << JSON_ATTRVAL_MASK << "\":\""
	    << std::setw(2) << std::setfill('0') << std::hex
	    << (int)mask << "\"";
	out << std::setfill(' ') << std::dec;
    }
    if (bias != 0){
	out << ",\"" << JSON_ATTRVAL_BIAS << "\":" << bias;
    }
    if (mulFactor != 1){
	out << ",\"" << JSON_ATTRVAL_MUL << "\":" << mulFactor;
    }
    if (divFactor != 1){
	out << ",\"" << JSON_ATTRVAL_DIV << "\":" << divFactor;
    }
    out << "}";
}

float PortionNode::extract(const IRCommand* cmd){
    auto rc = std::numeric_limits<float>::quiet_NaN();
    if ((cmd->bits + 7) / 8 >= offset){
	rc = (((uint8_t*)cmd->data)[offset] & mask);
	rc = rc * mulFactor / divFactor + bias;
    }
    return rc;
}

//----------------------------------------------------------------------
// add value node
//----------------------------------------------------------------------
class AddValueNode : public ValueFormulaNode{
protected:
    using Base = ValueFormulaNode;
    std::vector<Base::Ptr> children;

public:
    AddValueNode(): Base(Base::ADD){};
    virtual ~AddValueNode(){};
    bool deserialize(const json11::Json& in, std::string& err) override;
    void serialize(std::ostream& out) override;
    float extract(const IRCommand* cmd) override;
};

bool AddValueNode::deserialize(const json11::Json& in, std::string& err){
    auto elements = in[JSON_ATTRVAL_CHILDREN];
    if (elements.is_array()){
	for (auto &f : elements.array_items()){
	    if (!f.is_object()){
		err = "Sub-formula of add value node must be "
		      "specified as JSON object.";
		return false;
	    }
	    typename Base::Ptr child;
	    createValueFormula(json11::Json(f.object_items()), err, child);
	    if (child.isNull()){
		return false;
	    }
	    children.push_back(child);
	}
    }
    if (children.size() == 0){
	err = "At least one sub-formula must be specified "
	      "for add value node.";
	return false;
    }
    return true;
}

void AddValueNode::serialize(std::ostream& out){
    out << "{";
    Base::serialize(out);
    out << ",\"" << JSON_ATTRVAL_CHILDREN << "\":[";
    for (auto i = children.begin(); i != children.end(); i++){
	if (i != children.begin()){
	    out << ",";
	}
	(*i)->serialize(out);
    }
    out << "]}";
}

float AddValueNode::extract(const IRCommand* cmd){
    float rc = 0;
    for (auto i = children.begin(); i != children.end(); i++){
	rc += (*i)->extract(cmd);
    }
    return rc;
}

//----------------------------------------------------------------------
// value formula node factory
//----------------------------------------------------------------------
static void createValueFormula(const json11::Json& in, std::string& err,
			       ValueFormulaNode::Ptr& ptr){
    using Formula = ValueFormulaNode;
    auto type = in[JSON_ATTRVAL_TYPE];
    ptr = Formula::Ptr(NULL);
    
    if (type.is_string()){
	auto typeStr = type.string_value();
	if (typeStr == VFORMULA_TYPE_STR[Formula::ADD]){
	    ptr = Formula::Ptr(new AddValueNode);
	}else if (typeStr == VFORMULA_TYPE_STR[Formula::PORTION]){
	    ptr = Formula::Ptr(new PortionNode);
	}else{
	    err = "Invalid value formula type was specified: ";
	    err += typeStr;
	}
	if (!ptr->deserialize(in, err)){
	    ptr = Formula::Ptr(NULL);
	}
    }else{
	err = "No value formula type was specified.";
    }
}

//======================================================================
// attribute inmplementation
//======================================================================
class AttributeImp : public ShadowDevice::Attribute{
protected:
    using Base = ShadowDevice::Attribute;
    const ShadowDevice* shadow;

    bool dirtyFlag;
    
    float numericValue;
    std::string* stringValue;
    float numericValueBkup;
    std::string* stringValueBkup;
    
    bool notApplyInOff;
    float defaultValue;
    float valueMax;
    float valueMin;
    float valueUnit;
    ValueFormulaNode::Ptr valueFormula;
    std::map<float, std::string> dict;
    FormulaNode<IRCommand>::Ptr applyCondition;
    FormulaNode<ShadowDevice>::Ptr visibleCondition;
    
public:    
    using Ptr = SmartPtr<AttributeImp>;

    AttributeImp(const ShadowDevice* dev):
	shadow(dev), dirtyFlag(false),
	numericValue(0), stringValue(NULL),
	numericValueBkup(0), stringValueBkup(NULL),
	notApplyInOff(false),
	defaultValue(std::numeric_limits<float>::quiet_NaN()),
	valueMax(std::numeric_limits<float>::quiet_NaN()),
	valueMin(std::numeric_limits<float>::quiet_NaN()),
	valueUnit(std::numeric_limits<float>::quiet_NaN()){};
    virtual ~AttributeImp(){};

    bool isDirty()const {return dirtyFlag;};
    void resetDirtyFlag(){dirtyFlag = false;};
    
    bool deserialize(const json11::Json& in, std::string& err);
    void serialize(std::ostream& out, const std::string& name);
    bool applyIRCommand(const IRCommand* cmd);
    bool isVisible()const override;
    bool printKV(std::ostream& out, const std::string& name,
		 bool needSep)const override;

    float getNumericValue()const override{
	return numericValue;
    };
    const std::string& getStringValue()const override{
	static auto nullstr = std::string();
	return stringValue ? *stringValue : nullstr;
    };

    void backup(){
	numericValueBkup = numericValue;
	stringValueBkup = stringValue;
    };
    void restore(){
	numericValue = numericValueBkup;
	stringValue = stringValueBkup;
    };
    bool setNumericValue(float value);
    bool setStringValue(const std::string& value);
};

bool AttributeImp::deserialize(const json11::Json& in, std::string& err){
    ApplyBoolValue(in, JSON_ATTR_NOAPPINOFF, true, [&](bool v){
	    this->notApplyInOff = v;
	    return true;
	});

    //
    // Value Formula
    //
    auto vformula = in[JSON_ATTR_VALUE];
    if (vformula.is_object()){
	createValueFormula(json11::Json(vformula.object_items()),
			   err, valueFormula);
	if (valueFormula.isNull()){
	    return false;
	}
    }else{
	err = "Formula to extract attribute value "
	      "must be specified as object.";
	return false;
    }

    //
    // Dictionary
    //
    auto dictDefs = in[JSON_ATTR_DICT];
    if (dictDefs.is_object()){
	const char*  errStr = "Dictionary value must be numeric or "
	                      "string presented 1 byte hex.";
	for (auto &kv : dictDefs.object_items()){
	    float key = std::numeric_limits<float>::quiet_NaN();
	    if (kv.second.is_string()){
		auto hex = strToHex(kv.second.string_value());
		if (hex.length() != 1){
		    err = errStr;
		    return false;
		}
		key = hex[0];
	    }else if (kv.second.is_number()){
		key = kv.second.number_value();
	    }else{
		err = errStr;
		return false;
	    }
	    dict[key] = kv.first;
	}
    }

    //
    // Default Value
    //
    auto defaultDef = in[JSON_ATTR_DEFAULT];
    const char* defError = "Default value must indicate one of imtems "
	                   "in the dictionary";
    if (defaultDef.is_string()){
	auto value = defaultDef.string_value();
	float key = std::numeric_limits<float>::quiet_NaN();
	for (auto &i : dict){
	    if (i.second == value){
		key = i.first;
		break;
	    }
	}
	if (std::isnan(key)){
	    err = defError;
	    return false;
	}
	defaultValue = key;
    }else if (defaultDef.is_number()){
	defaultValue = defaultDef.number_value();
	if (!dict.empty()){
	    auto i = dict.find(defaultValue);
	    if (i == dict.end()){
		err = defError;
		return false;
	    }
	}
    }
    if (!std::isnan(defaultValue)){
	numericValue = defaultValue;
	auto i = dict.find(numericValue);
	if (i != dict.end()){
	    stringValue = &(i->second);
	}
    }

    //
    // Value Range
    //
    ApplyNumValue(in, JSON_ATTR_MAX, true, [&](float v){
	    valueMax = v;
	    return true;
	});
    ApplyNumValue(in, JSON_ATTR_MIN, true, [&](float v){
	    valueMin = v;
	    return true;
	});
    ApplyNumValue(in, JSON_ATTR_UNIT, true, [&](float v){
	    valueUnit = v;
	    return true;
	});

    //
    // Apply Condition
    //
    auto acondDefs = in[JSON_ATTR_APPLY_COND];
    if (acondDefs.is_object()){
	createFormula(json11::Json(acondDefs.object_items()), err,
		      applyCondition);
	if (applyCondition.isNull()){
	    return false;
	}
    }

    //
    // Visible Condition
    //
    auto vcondDefs = in[JSON_ATTR_VISIBLE_COND];
    if (vcondDefs.is_object()){
	createFormula(json11::Json(vcondDefs.object_items()), err,
		      visibleCondition);
	if (visibleCondition.isNull()){
	    return false;
	}
    }
    
    return true;
}

void AttributeImp::serialize(std::ostream& out, const std::string& name){
    out << "{\"" << JSON_ATTR_NAME << "\":\"" << name << "\"";
    if (notApplyInOff){
	out << ",\"" << JSON_ATTR_NOAPPINOFF << "\":true";
    }
    out << ",\"" << JSON_ATTR_VALUE << "\":";
    valueFormula->serialize(out);
    if (!dict.empty()){
	out << ",\"" << JSON_ATTR_DICT << "\":{";
	for (auto i = dict.begin(); i != dict.end(); i++){
	    out << (i == dict.begin() ? "\"" : ",\"")
		<< i->second << "\":" << i->first;
	}
	out << "}";
    }
    if (!std::isnan(defaultValue)){
	out << ",\"" << JSON_ATTR_DEFAULT << "\":";
	if (dict.empty()){
	    out << defaultValue;
	}else{
	    auto v = dict.at(defaultValue);
	    out << "\"" << v << "\"";
	}
    }
    if (!std::isnan(valueMax)){
	    out << ",\"" << JSON_ATTR_MAX << "\":" << valueMax;
	}
    if (!std::isnan(valueMin)){
	    out << ",\"" << JSON_ATTR_MIN << "\":" << valueMin;
	}
    if (!std::isnan(valueUnit)){
	    out << ",\"" << JSON_ATTR_UNIT << "\":" << valueUnit;
	}
    if (!applyCondition.isNull()){
	out << ",\"" << JSON_ATTR_APPLY_COND << "\":";
	applyCondition->serialize(out);
    }
    if (!visibleCondition.isNull()){
	out << ",\"" << JSON_ATTR_VISIBLE_COND << "\":";
	visibleCondition->serialize(out);
    }
    out << "}";
}

bool AttributeImp::applyIRCommand(const IRCommand* cmd){
    if (notApplyInOff && !shadow->isOn()){
	ESP_LOGD(tag, "false: notApplyInOff");
	return false;
    }
    if (!applyCondition.isNull() && !applyCondition->test(cmd)){
	ESP_LOGD(tag, "false: apply condition");
	return false;
    }
    auto value = valueFormula->extract(cmd);
    ESP_LOGD(tag, "value: %f", value);
    if (std::isnan(value)){
	ESP_LOGD(tag, "false: value is NaN");
	return false;
    }
    auto oldValue = numericValue;
    if (dict.empty()){
	numericValue = value;
    }else{
	auto i = dict.find(value);
	if (i == dict.end()){
	    ESP_LOGD(tag, "false: no muched item in dictionary");
	    return false;
	}
	numericValue = value;
	stringValue = &(i->second);
    }
    if (oldValue != numericValue){
	dirtyFlag = true;
    }

    ESP_LOGD(tag, "true: applied");
    return true;
}

bool AttributeImp::isVisible()const {
    return visibleCondition.isNull() || visibleCondition->test(shadow);
}

bool AttributeImp::printKV(std::ostream& out, const std::string& name,
			   bool needSep) const{
    if (visibleCondition.isNull() || visibleCondition->test(shadow)){
	if (needSep){
	    out << ",";
	}
	out << "\"" << name << "\":";
	if (stringValue){
	    out << "\"" << *stringValue << "\"";
	}else{
	    out << numericValue;
	}
	return true;
    }else{
	return false;
    }
}

bool AttributeImp::setNumericValue(float value){
    if (!std::isnan(valueMax) && value > valueMax){
	return false;
    }
    if (!std::isnan(valueMin) && value < valueMin){
	return false;
    }
    if (!std::isnan(valueUnit)){
	double integer;
	if (modf(value / valueUnit, &integer) != 0){
	    return false;
	}
    }
    numericValue = value;
    ESP_LOGD(tag, "set attribute value: %f", numericValue);
    return true;
}

bool AttributeImp::setStringValue(const std::string& value){
    for (auto &kv : dict){
	if (kv.second == value){
	    stringValue = &(kv.second);
	    numericValue = kv.first;
	    return true;
	}
    }
    return false;
}

//======================================================================
// synthesizer variable formula node
//======================================================================

//----------------------------------------------------------------------
// base class
//----------------------------------------------------------------------
class SvFormulaNode {
public:
    enum Type {INTEGER, DECIMAL, POWER, ATTRIBUTE, CONSTANT};
    using Ptr = SmartPtr<SvFormulaNode>;

protected:
    const Type type;

public:
    SvFormulaNode(Type type):type(type){};
    virtual ~SvFormulaNode(){};
    
    Type getType() const {return type;};

    virtual bool deserialize(const json11::Json& in, std::string& err) = 0;
    virtual void serialize(std::ostream& out){
	out << "\"" << JSON_SYNVAL_TYPE << "\":\""
	    << SYNVAL_TYPE_STR[type] << "\"";
    };
    virtual float getValue(const ShadowDevice* shadow) = 0;
};

static void createSvFormula(const json11::Json& in, std::string& err,
			    SvFormulaNode::Ptr& ptr);

//----------------------------------------------------------------------
// monadic operator
//----------------------------------------------------------------------
class SvMonadicOperator : public SvFormulaNode{
protected:
    using Base = SvFormulaNode;
    Base::Ptr subFormula;

public:
    SvMonadicOperator(Base::Type type):Base(type){};
    virtual ~SvMonadicOperator(){};

    bool deserialize(const json11::Json& in, std::string& err) override;
    void serialize(std::ostream& out) override;
    float getValue(const ShadowDevice* shadow) override;
};

bool SvMonadicOperator::deserialize(const json11::Json& in, std::string& err){
    auto sub = in[JSON_SYNVAL_SUBFORMULA];
    if (sub.is_object()){
	createSvFormula(json11::Json(sub.object_items()), err, subFormula);
	if (subFormula.isNull()){
	    return false;
	}
    }else{
	err = "A sub-furmula must be specified for monadic operator.";
	return false;
    }
    return true;
}

void SvMonadicOperator::serialize(std::ostream& out){
    out << "{";
    Base::serialize(out);
    out << ",\"" << JSON_SYNVAL_SUBFORMULA << "\":";
    subFormula->serialize(out);
    out << "}";
}

float SvMonadicOperator::getValue(const ShadowDevice* shadow){
    double integer;
    float decimal = modf( subFormula->getValue(shadow), &integer);
    if (getType() == Base::INTEGER){
	return integer;
    }else if (getType() == Base::DECIMAL){
	return decimal;
    }else{
	return std::numeric_limits<float>::quiet_NaN();
    }
};

//----------------------------------------------------------------------
// power status node
//----------------------------------------------------------------------
class SvPower : public SvFormulaNode{
protected:
    using Base = SvFormulaNode;

public:
    SvPower():Base(Base::POWER){};
    virtual ~SvPower(){};

    bool deserialize(const json11::Json& in, std::string& err) override;
    void serialize(std::ostream& out) override;
    float getValue(const ShadowDevice* shadow) override;
};

bool SvPower::deserialize(const json11::Json& in, std::string& err){
    return true;
}

void SvPower::serialize(std::ostream& out){
    out << "{";
    Base::serialize(out);
    out << "}";
}

float SvPower::getValue(const ShadowDevice* shadow){
    return shadow->isOn() ? 1 : 0;
}

//----------------------------------------------------------------------
// attribute value node
//----------------------------------------------------------------------
class SvAttribute : public SvFormulaNode{
protected:
    using Base = SvFormulaNode;
    std::string attrName;

public:
    SvAttribute():Base(Base::ATTRIBUTE){};
    virtual ~SvAttribute(){};

    bool deserialize(const json11::Json& in, std::string& err) override;
    void serialize(std::ostream& out) override;
    float getValue(const ShadowDevice* shadow) override;
};

bool SvAttribute::deserialize(const json11::Json& in, std::string& err){
    if (!ApplyValue(in, JSON_SYNVAL_ATTRNAME, false, [&](const std::string& v){
		this->attrName = v;
		return true;
	    })){
	err = "Attribute name must be specified for Attribute node.";
	return false;
    }
    return true;
}

void SvAttribute::serialize(std::ostream& out){
    out << "{";
    Base::serialize(out);
    out << ",\"" << JSON_SYNVAL_ATTRNAME << "\":\"" << attrName << "\"";
    out << "}";
}

float SvAttribute::getValue(const ShadowDevice* shadow){
    auto attr = shadow->getAttribute(attrName);
    if (attr == nullptr){
	return std::numeric_limits<float>::quiet_NaN();
    }
    return attr->getNumericValue();
}

//----------------------------------------------------------------------
// constant value node
//----------------------------------------------------------------------
class SvConstant : public SvFormulaNode{
protected:
    using Base = SvFormulaNode;
    float value;

public:
    SvConstant():Base(Base::CONSTANT),
		 value(std::numeric_limits<float>::quiet_NaN()){};
    virtual ~SvConstant(){};

    bool deserialize(const json11::Json& in, std::string& err) override;
    void serialize(std::ostream& out) override;
    float getValue(const ShadowDevice* shadow) override{return value;};
};

bool SvConstant::deserialize(const json11::Json& in, std::string& err){
    auto json = in[JSON_SYNVAL_CONSTVAL];
    if (json.is_string()){
	auto hex = strToHex(json.string_value());
	if (hex.length() == 1){
	    value = hex[0];
	}
    }else if (json.is_number()){
	value = json.number_value();
    }
    if (std::isnan(value)){
	err = "Value must be specified as numeric or one byte hex string "
	      "for constant node.";
	return false;
    }
    return true;
}

void SvConstant::serialize(std::ostream& out){
    out << "{";
    Base::serialize(out);
    out << ",\"" << JSON_SYNVAL_CONSTVAL << "\":" << value;
    out << "}";
}

//----------------------------------------------------------------------
// synthesizer variable formula node factory
//----------------------------------------------------------------------
static void createSvFormula(const json11::Json& in, std::string& err,
			    SvFormulaNode::Ptr& ptr){
    ptr.setNull();
    using Formula = SvFormulaNode;
    auto type = in[JSON_SYNVAL_TYPE];
    
    if (type.is_string()){
	auto typeStr = type.string_value();
	if (typeStr == SYNVAL_TYPE_STR[Formula::INTEGER]){
	    ptr = Formula::Ptr(new SvMonadicOperator(Formula::INTEGER));
	}else if (typeStr == SYNVAL_TYPE_STR[Formula::DECIMAL]){
	    ptr = Formula::Ptr(new SvMonadicOperator(Formula::DECIMAL));
	}else if (typeStr == SYNVAL_TYPE_STR[Formula::POWER]){
	    ptr = Formula::Ptr(new SvPower);
	}else if (typeStr == SYNVAL_TYPE_STR[Formula::ATTRIBUTE]){
	    ptr = Formula::Ptr(new SvAttribute);
	}else if (typeStr == SYNVAL_TYPE_STR[Formula::CONSTANT]){
	    ptr = Formula::Ptr(new SvConstant);
	}else{
	    err = "Invalid synthesizer variable value type was specified.";
	}
	if (!ptr->deserialize(in, err)){
	    ptr.setNull();
	}
    }else{
	err = "No synthesizer variable value type was specified.";
    }
}

//======================================================================
// redundant code generator inmplementation
//======================================================================
class RedundantCode{
public:
    enum Type{CHECKSUM, SUB4BIT};
    using Ptr = SmartPtr<RedundantCode>;
protected:
    Type type;
    int32_t offset;
    int32_t seed;
public:
    RedundantCode(): offset(-1), seed(0){};
    virtual ~RedundantCode(){};

    virtual bool deserialize(const json11::Json& in, std::string& err);
    virtual void serialize(std::ostream& out);
    virtual bool addRedundantCode(std::string& code);
};

bool RedundantCode::deserialize(const json11::Json& in, std::string& err){
    if (!ApplyValue(in, JSON_SYNRED_OPERATOR, false, [&](const std::string& v){
		for (int i = 0; SYN_REDUNDANT_TYPE_STR[i]; i++){
		    if (v == SYN_REDUNDANT_TYPE_STR[i]){
			this->type = (Type)i;
			return true;
		    }
		}
		return false;
	    })){
	err = "Invarid rudundant operator type or "
	    "no operator type was specified.";
	return false;
    }
    ApplyValue(in, JSON_SYNRED_OFFSET, true, [&](int32_t v){
	    this->offset = v;
	    return true;
	});

    auto json = in[JSON_SYNRED_SEED];
    if (json.is_string()){
	auto hex = strToHex(json.string_value());
	if (hex.length() == 1){
	    err = "Seed of redundant coding must be numeric or "
		  "one byte hex data as string.";
	}
	seed = hex[0];
    }else if (json.is_number()){
	seed = json.number_value();
    }
	
    return true;
}

void RedundantCode::serialize(std::ostream& out){
    out << "{\"" << JSON_SYNRED_OPERATOR << "\":\""
	<< SYN_REDUNDANT_TYPE_STR[type] << "\"";
    if (offset >= 0){
	out << ",\"" << JSON_SYNRED_OFFSET << "\":" << offset;
    }
    if (seed != 0){
	out << ",\"" << JSON_SYNRED_SEED << "\":" << seed;
    }
    out << "}";
}

bool RedundantCode::addRedundantCode(std::string& code){
    auto offset = this->offset;
    if (offset < 0){
	offset = code.length() - 1;
    }
    if (code.length() <= offset){
	return false;
    }

    int32_t rc = seed;
    if (type == CHECKSUM){
	for (int i = 0; i < offset; i++){
	    rc += code[i];
	}
    }else if (type == SUB4BIT){
	int32_t hrc = 0xf0 & seed;
	int32_t lrc = 0x0f & seed;
	for (int i = 0; i < offset; i++){
	    int32_t hh = 0xf0 & (uint8_t)code[i];
	    int32_t lh = 0x0f & (uint8_t)code[i];
	    hrc -= hh;
	    lrc -= lh;
	}
	rc = (hrc & 0xf0) | (lrc & 0x0f);
    }
    code[offset] = rc;
    
    return true;
}

//======================================================================
// synthesizer variable
//======================================================================
class SynVariable{
protected:
    int32_t offset;
    uint8_t mask;
    float bias;
    float mulFactor;
    float divFactor;
    std::string relativeAttribute;
    SvFormulaNode::Ptr value;
    
public:
    using Ptr = SmartPtr<SynVariable>;
    SynVariable(): offset(0), mask(0xff),
		   bias(0), mulFactor(1), divFactor(1){};
    virtual ~SynVariable(){};

    virtual bool deserialize(const json11::Json& in, std::string& err);
    virtual void serialize(std::ostream& out);
    virtual bool isApplicable(const ShadowDevice* shadow);
    virtual bool apply(const ShadowDevice* shadow, std::string& code);
};

bool SynVariable::deserialize(const json11::Json& in, std::string& err){
    if (!ApplyValue(in, JSON_SYNVER_OFFSET, false, [&](int32_t v){
		this->offset = v;
		return true;
	    })){
	err = "Offset must be specified for synthesizer variable.";
	return false;
    }
    if (!ApplyHexValue(in, JSON_SYNVER_MASK, true, [&](const std::string& v){
		if (v.length() != 1){
		    return false;
		}
		this->mask = v[0];
		return true;
	    })){
	err = "Mask in synthesizer variable must be "
	      "one byte hex data as string.";
	return false;
    }
    ApplyNumValue(in, JSON_SYNVER_BIAS, true, [&](float v){
	    bias = v;
	    return true;
	});
    ApplyNumValue(in, JSON_SYNVER_DIV, true, [&](float v){
	    divFactor = v;
	    return true;
	});
    ApplyNumValue(in, JSON_SYNVER_MUL, true, [&](float v){
	    mulFactor = v;
	    return true;
	});
    ApplyValue(in, JSON_SYNVER_RELATTR, true, [&](const std::string& v){
	    this->relativeAttribute = v;
	    return true;
	});

    auto valDef = in[JSON_SYNVER_VALUE];
    if (valDef.is_object()){
	createSvFormula(json11::Json(valDef.object_items()), err, value);
	if (value.isNull()){
	    return false;
	}
    }
    return true;
}

void SynVariable::serialize(std::ostream& out){
    out << "{\"" << JSON_SYNVER_OFFSET << "\":" << offset;
    if (mask != 0xff){
	out << ",\"" << JSON_SYNVER_MASK << "\":\""
	    << std::setw(2) << std::setfill('0') << std::hex
	    << (int)mask << "\"";
	out << std::setfill(' ') << std::dec;
    }
    if (bias != 0){
	out << ",\"" << JSON_SYNVER_BIAS << "\":" << bias;
    }
    if (mulFactor != 1){
	out << ",\"" << JSON_SYNVER_MUL << "\":" << mulFactor;
    }
    if (divFactor != 1){
	out << ",\"" << JSON_SYNVER_DIV << "\":" << divFactor;
    }
    if (!value.isNull()){
	out << ",\"" << JSON_SYNVER_VALUE << "\":";
	value->serialize(out);
    }
    if (relativeAttribute.length() > 0){
	out << ",\"" << JSON_SYNVER_RELATTR << "\":\""
	    << relativeAttribute << "\"";
    }
    out << "}";
}

bool SynVariable::isApplicable(const ShadowDevice* shadow){
    if (relativeAttribute.length() > 0){
	auto attr = shadow->getAttribute(relativeAttribute);
	if (!attr){
	    return false;
	}
	return attr->isVisible();
    }
    return true;
}

bool SynVariable::apply(const ShadowDevice* shadow, std::string& code){
    ESP_LOGD(tag, "offset: %d, mask: %.2x", offset, mask);
    if (!isApplicable(shadow)){
	ESP_LOGD(tag, "not applicable");
	return false;
    }

    float v = (value->getValue(shadow) + bias) * mulFactor / divFactor;
    int32_t iv = (int32_t)v;
    int32_t byte = (iv & mask) | (code[offset] & ~mask);
    code[offset] = byte;
    ESP_LOGD(tag, "applied: %.2x : %.2x", iv, byte);
    
    return true;
}

//======================================================================
// synthesizer implementation
//======================================================================
class Synthesizer{
protected:
    IRRC_PROTOCOL protocol;
    std::string placeholder;
    int32_t bits;
    std::vector<SynVariable::Ptr> variables;
    RedundantCode::Ptr redundant;

public:
    using Ptr = SmartPtr<Synthesizer>;
    Synthesizer():bits(0){};
    virtual ~Synthesizer(){};

    bool deserialize(const json11::Json& in, std::string& err);
    void serialize(std::ostream& out);
    void issueIRCommand(const ShadowDevice* shadow);
};

bool Synthesizer::deserialize(const json11::Json& in, std::string& err){
    if (!ApplyValue(in, JSON_SYN_PROTOCOL, false,
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
    if (!ApplyHexValue(in, JSON_SYN_PLACEHOLDER, true,
		       [&](std::string& v)->bool{
			   this->placeholder = std::move(v);
			   return true;
		       })){
	err = "Invalid hex data was specified for synthesizer placeholder.";
	return false;
    }
    ApplyValue(in, JSON_SYN_BITS, true, [&](int32_t v) -> bool{
	    this->bits = v;
	    return true;
	});
    if (placeholder.length() == 0 && bits <= 0){
	err = "Placeholder or bit count must be specified "
	      "for synthsizer definition at least.";
	return false;
    }

    auto valsDef = in[JSON_SYN_VARS];
    if (valsDef.is_array()){
	for (auto &v : valsDef.array_items()){
	    if (!v.is_object()){
		err = "Variable definition of synthesizer must be "
		      "JSON object.";
		return false;
	    }
	    SynVariable::Ptr variable(new SynVariable);
	    if (!variable->deserialize(json11::Json(v.object_items()), err)){
		return false;
	    }
	    variables.push_back(variable);
	}
    }

    auto redundantDef = in[JSON_SYN_REDUNDANT];
    if (redundantDef.is_object()){
	redundant = RedundantCode::Ptr(new RedundantCode);
	if (!redundant->deserialize(
		json11::Json(redundantDef.object_items()), err)){
	    return false;
	}
    }

    return true;
}

void Synthesizer::serialize(std::ostream& out){
    out << "{\"" << JSON_SYN_PROTOCOL << "\":\""
	<< PROTOCOL_STR[(int)protocol] << "\"";
    if (placeholder.length() > 0){
	out << ",\"" << JSON_SYN_PLACEHOLDER << "\":\"";
	printhex(out, placeholder);
	out << "\"";
    }
    if (bits > 0){
	out << ",\"" << JSON_SYN_BITS << "\":" << bits;
    }
    if (variables.size() > 0){
	out << ",\"" << JSON_SYN_VARS << "\":[";
	for (auto v = variables.begin(); v != variables.end(); v++){
	    if (v != variables.begin()){
		out << ",";
	    }
	    (*v)->serialize(out);
	}
	out << "]";
    }
    if (!redundant.isNull()){
	out << ",\"" << JSON_SYN_REDUNDANT << "\":";
	redundant->serialize(out);
    }
    out << "}";
}

void Synthesizer::issueIRCommand(const ShadowDevice* shadow){
    std::string cmd;
    if (placeholder.length() > 0){
	cmd = placeholder;
    }else{
	cmd.assign((bits + 7) / 8, 0);
    }
    auto bits = this->bits;
    if (bits <= 0){
	bits = cmd.length() * 8;
    }
    for (auto &v : variables){
	v->apply(shadow, cmd);
    }
    if (!redundant.isNull()){
	redundant->addRedundantCode(cmd);
    }

    sendIRData(protocol, bits, (uint8_t*)cmd.data());
    shadowStat.synthesizeCount++;
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
    std::map<std::string, AttributeImp::Ptr> attributes;
    Synthesizer::Ptr synthesizer;
    Synthesizer::Ptr synthesizerToOff;

    bool powerStatus;
    bool powerStatusBkup;

public:
    ShadowDeviceImp(const char* name):name(name),
				      powerStatus(false),
				      powerStatusBkup(false){};
    virtual ~ShadowDeviceImp(){};

    bool isDirty() const override{
	auto rc = false;
	for (auto i =  attributes.begin(); i != attributes.end(); i++){
	    rc = (rc || i->second->isDirty());
	}
	return rc;
    };
    void resetDirtyFlag() override{
	for (auto i =  attributes.begin(); i != attributes.end(); i++){
	    i->second->resetDirtyFlag();
	}
    };

    const std::string& getName() const{return name;};

    bool deserialize(const json11::Json& in, std::string& err);
    void serialize(std::ostream& out) override;
    bool applyIRCommand(const IRCommand* cmd);
        
    bool isOn()const override;
    void setPowerStatus(bool isOn) override;
    void dumpStatus(std::ostream& out) override;
    const Attribute* getAttribute(const std::string& name)const override;
    bool setStatus(const json11::Json& json, std::string& err,
		   bool ignorePower = false) override;
    
protected:
    void backup();
    void restore();
    bool applyIRCommandToSW(const IRCommand* cmd);
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

    auto attrDefs = in[JSON_SHADOW_ATTRIBUTES];
    if (attrDefs.is_array()){
	for (auto &def : attrDefs.array_items()){
	    if (def.is_object()){
		auto obj = json11::Json(def.object_items());
		auto name = obj[JSON_ATTR_NAME];
		if (!name.is_string()){
		    err = "Attribute name must be specified.";
		    return false;
		}
		AttributeImp::Ptr ptr(new AttributeImp(this));
		if (!ptr->deserialize(obj, err)){
		    return false;
		}
		attributes[name.string_value()] = ptr;;
	    }
	}
    }

    auto synDef = in[JSON_SHADOW_SYN];
    if (synDef.is_object()){
	synthesizer = Synthesizer::Ptr(new Synthesizer);
	if (!synthesizer->deserialize(
		json11::Json(synDef.object_items()), err)){
	    return false;
	}
    }
    synDef = in[JSON_SHADOW_SYNOFF];
    if (synDef.is_object()){
	synthesizerToOff = Synthesizer::Ptr(new Synthesizer);
	if (!synthesizerToOff->deserialize(
		json11::Json(synDef.object_items()), err)){
	    return false;
	}
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
    if (!attributes.empty()){
	out << ",\"" << JSON_SHADOW_ATTRIBUTES << "\":[";
	for (auto i = attributes.begin(); i != attributes.end(); i++){
	    if (i != attributes.begin()){
		out << ",";
	    }
	    i->second->serialize(out, i->first);
	}
	out << "]";
    }
    if (!synthesizer.isNull()){
	out << ",\"" << JSON_SHADOW_SYN << "\":";
	synthesizer->serialize(out);
    }
    if (!synthesizerToOff.isNull()){
	out << ",\"" << JSON_SHADOW_SYNOFF << "\":";
	synthesizerToOff->serialize(out);
    }
    out << "}";
}

bool ShadowDeviceImp::applyIRCommand(const IRCommand* cmd){
    auto rc = applyIRCommandToSW(cmd);
    if (rc){
	shadowStat.recognizeCount++;
	for (auto &attr : attributes){
	    ESP_LOGD(tag, "apply attr: %s", attr.first.c_str());
	    attr.second->applyIRCommand(cmd);
	}
    }
    return rc;
}

bool ShadowDeviceImp::applyIRCommandToSW(const IRCommand* cmd){
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

bool ShadowDeviceImp::isOn() const{
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
	<< (isOn() ? "true" : "false");
    if (!attributes.empty()){
	out << ",\"" << JSON_SHADOW_ATTRIBUTES << "\":{";
	bool needSep = false;
	for (auto attr = attributes.begin(); attr != attributes.end(); attr++){
	    needSep |= attr->second->printKV(out, attr->first, needSep);
	}
	out << "}";
    }
    out << "}";
}

const ShadowDevice::Attribute* ShadowDeviceImp::getAttribute(
    const std::string& name) const{
    auto i = attributes.find(name);
    if (i == attributes.end()){
	return NULL;
    }else{
	return i->second;
    }
}

void ShadowDeviceImp::backup(){
    powerStatusBkup = powerStatus;
    for (auto &attr : attributes){
	attr.second->backup();
    }
}

void ShadowDeviceImp::restore(){
    powerStatus = powerStatusBkup;
    for (auto &attr : attributes){
	attr.second->restore();
    }
}

bool ShadowDeviceImp::setStatus(const json11::Json& status, std::string& err,
				bool ignorePower){
    class ValueCommiter{
    protected:
	ShadowDeviceImp* shadow;
	bool commited;
    public:
	ValueCommiter(ShadowDeviceImp* shadow):shadow(shadow), commited(false){
	    shadow->backup();
	};
	~ValueCommiter(){
	    if (!commited){
		shadow->restore();
	    }
	};
	void commit(){commited = true;}
    };
    ValueCommiter commiter(this);

    ESP_LOGI(tag, "changing shadow '%s' status", name.c_str());
    
    //
    // apply power status
    //
    if (!ignorePower){
	ApplyBoolValue(status, JSON_SHADOW_ISON, true, [&](bool v){
		this->powerStatus = v;
		return true;
	    });
    }
    
    //
    // apply attribute value
    //
    auto attrs = status[JSON_SHADOW_ATTRIBUTES];
    if (attrs.is_object()){
	for (auto &kv : attrs.object_items()){
	    auto attr = attributes.find(kv.first);
	    if (attr == attributes.end()){
		err = "There isn't specified attribute: ";
		err += kv.first;
		return false;
	    }
	    if (kv.second.is_string()){
		auto sv = kv.second.string_value();
		if (!attr->second->setStringValue(sv)){
		    err = "Specified attribute value is not included in "
			  "attribute dictionary: ";
		    err += sv;
		    return false;
		}
	    }else if (kv.second.is_number()){
		auto nv = kv.second.number_value();
		if (!attr->second->setNumericValue(nv)){
		    err = "Specified attribute value is out of range: ";
		    err += kv.first;
		    return false;
		}
	    }
	}
    }

    //
    // synthesize IR command and issue IR command
    //
    if (!ignorePower){
	if (!powerStatus && !synthesizerToOff.isNull()){
	    synthesizerToOff->issueIRCommand(this);
	}else if (!synthesizer.isNull()){
	    synthesizer->issueIRCommand(this);
	}
    }

    commiter.commit();
    return true;
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

    auto apath = elfletConfig->getShadowStatusPath();
    rc = stat(apath, &sbuf);
    if (rc != 0){
	return true;
    }
    length = sbuf.st_size;
    buf = new char[length];
    fp = fopen(apath, "r");
    rc = fread(buf, length, 1, fp);
    if (rc != 1){
	ESP_LOGE(tag, "cannot read shadow status file: %s", apath);
	fclose(fp);
	delete buf;
	return false;
    }
    fclose(fp);
    std::string scontents(buf, length);
    delete buf;
    auto statuses = json11::Json::parse(scontents, err);
    if (!statuses.is_array()){
	ESP_LOGE(tag, "shadow status file corrupted: %s", apath);
	return true;
    }
    int num = 0;
    for (auto &status : statuses.array_items()){
	auto shadow =
	    findShadowDevice(status[JSON_SHADOW_NAME].string_value());
	if (shadow == NULL){
	    continue;
	}
	shadow->setStatus(status, err, true);
	num++;
    }
    
    return true;
}

static bool saveShadowDevicePool(std::ostream& out){
    out << "[";
    for (auto i = shadows.begin(); i != shadows.end(); i++){
	if (i != shadows.begin()){
	    out << ",";
	}
	(*i)->serialize(out);
    }
    out << "]";
    return true;
}

static bool saveShadowDevicePool(){
    auto defspath = elfletConfig->getShadowDefsPath();
    auto out = std::ofstream(defspath);
    saveShadowDevicePool(out);
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
    if (remove(elfletConfig->getShadowStatusPath()) != 0){
	ESP_LOGE(tag, "fail to remove shadow status file");
    }
    return true;
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
	    saveShadowStatuses();
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

bool applyIRCommandToShadow(const IRCommand* cmd){
    for (auto i = shadows.begin(); i != shadows.end(); i++){
	if ((*i)->applyIRCommand(cmd)){
	    if (elfletConfig->getShadowTopic().length() > 0){
		publishShadowStatus(*i);
	    }
	    ESP_LOGI(tag, "shadow '%s' turned %s",
		     (*i)->getName().c_str(), (*i)->isOn() ? "on" : "off");
	    if ((*i)->isDirty()){
		saveShadowStatuses();
		(*i)->resetDirtyFlag();
		ESP_LOGI(tag,
			 "shadow '%s' attributes has been changed, "
			 "saved attributes", (*i)->getName().c_str());
	    }
	    return true;
	}
    }
    return false;
}

bool saveShadowStatuses(){
    auto path = elfletConfig->getShadowStatusPath();
    auto out = std::ofstream(path);
    out << '[';
    for (auto i = shadows.begin(); i != shadows.end(); i++){
	if (i != shadows.begin()){
	    out << ',';
	}
	(*i)->dumpStatus(out);
    }
    out << ']';
    out.close();
    return true;
}
