elflet REST specification
===
**NOTE:** This document is under construction. All interfaces are listed, however descriptions are still insufficient.

## Management

method | path | auth|req-body | resp-body | description
:----|:------:|:-------------:|:------------:|:-------------:|:-----------
GET|/manage/config|none|-|application/json| get elflet configuration
POST|/manage/config|digest|appliation/json|-| change elfelt configuration
GET|/manage/status|none|-|applilcation/json|get statistics and status
POST|/manage/otaupdate|digest|multipart/form|-|update firmware.<br>firmware image is sent as `image` parameta in multipart form.

## IR remote controller
method | path | auth|req-body | resp-body | description
:----|:------:|:-------------:|:------------:|:-------------:|:-----------
GET|/irrc/startReciever|none|-|-| If `IrrcRecieverMode` parameter of configuration is `OnDemand`, elflet start to recieve IR remote controller signal. You can confirm elflet is in this mode as blinking LED blue.<br>If `IrrcRecieverMode` parameter is `Continuous`, This request is ignored.
GET|/irrc/recievedData|none|-|application/json| get latest recieved IR remote controller code
POST|/irrc/send|none|application/json|-|sedn IR remote controller command

## Device Shadow
method | path | auth|req-body | resp-body | description
:----|:------:|:-------------:|:------------:|:-------------:|:-----------
GET|/shadowDefs|none|-|application/json|list registerd shadow names
GET|/shadowDefs/\<*NAME*>|none|-|application/json|show shadow definition
POST|shadowDefs/\<*NAME*>|digest|application/json|-|add shadow definition
DELETE|shadowDefs/\<*NAME*>|digest|-|-|remove shadow definition
GET|/shadow|none|-|application/json|list registerd shadow names
GET|/shadow/\<*NAME*>|none|-|application/json|get shadow status
POST|/shadow/\<*NAME*>|none|application/json|-|change shadow status

## Sensor

method | path | auth|req-body | resp-body | description
:----|:------:|:-------------:|:------------:|:-------------:|:-----------
GET|/sensor|none|-|application/json| get sonsors value

## BLE keyboard emuration
method | path | auth|req-body | resp-body | description
:----|:------:|:-------------:|:------------:|:-------------:|:-----------
POST|/blehid|none|application/json|-| send key code to a device which paired with elflet BLE keyboard
