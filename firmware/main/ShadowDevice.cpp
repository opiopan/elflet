#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <GeneralUtils.h>
#include <json11.hpp>
#include "SmartPtr.h"
#include "Config.h"
#include "ShadowDevice.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "ShadowDevice";

//----------------------------------------------------------------------
// element of condition formula
//----------------------------------------------------------------------
class FormulaNode {
public:
    enum Type {COMBINATION, PROTOCOL, DATA};

protected:
    const Type type;

public:
    FormulaNode(Type type):type(type){};
    Type getType() const {return type;};
};

//----------------------------------------------------------------------
// formula node factory
//----------------------------------------------------------------------
typedef SmartPtr<FormulaNode> FormulaNodePtr;
FormulaNodePtr createFormula(){
    return FormulaNodePtr(NULL);
}

