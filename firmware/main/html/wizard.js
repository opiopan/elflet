var currentPage = 0;
var pageCount = 4;
var clickEvent="click";

changePage(0);

$(window).on('load', function() {
    $('#next').on(clickEvent, function(){
	changePage(currentPage + 1);
    });
    $('#prev').on(clickEvent, function(){
	changePage(currentPage - 1);
    });
    $('#commit').on(clickEvent, function(){
	commitConfig();
    });
    $('.textInput').on('input', function(){
	updateNavigationStatus();
    });
    $('#startWizard').on(clickEvent, function(){
	$('#startWizardCover').attr('deactive', "");
    });

    $.get('manage/config', '', function(result){
	var config = null;
	if (typeof result === 'object'){
	    config = result;
	}else{
	    config = JSON.parse(result);
	}
	if (config.SSID){
	    $('input[name="ssid"]').val(config.SSID);
	}
	if (config.WiFiPassword){
	    $('input[name="ssidPass"]').val(config.WiFiPassword);
	}
	if (config.NodeName){
	    $('input[name="deviceName"]').val(config.NodeName);
	}
	updateNavigationStatus();
	$('#prepare').attr('deactive', "");
    });
});

function changePage(page){
    if (page < 0 || page >= pageCount){
	return;
    }
    if (!checkTransitionability()){
	return;
    }

    $('.pgsel').each(function(index, element){
	if (index == currentPage){
	    $(element).prop('checked', false);
	}
	if (index == page){
	    $(element).prop('checked', true);
	}
    });
    currentPage = page;
    updateNavigationStatus();
}

function checkTransitionability(){
    var config = getConfig();
    var msg = null;
    
    if (currentPage == 0){
	if (config.ssid.length > 32){
	    msg = "SSID must be less than 32 bytes.";
	}else if (config.ssidPass.length > 64){
	    msg = "Passphrase must be less than 64 bytes.";
	}
    }else if (currentPage == 1){
	if (config.name.length > 32){
	    msg = "Device name must be less than 32 bytes.";
	}
    }else if (currentPage == 2){
	var alen = config.password.length;
	var blen = config.passwordAgain.length;
	if ((alen != 0 && blen != 0) && 
	    (alen < 8 || alen > 64 || blen < 8 || blen > 64)){
	    msg = "Administrator password must be grater than 8 bytes "
	          "and must be less than 64 bytes.";
	}
    }
	
    if(msg){
	window.alert(msg);
	return false;
    }else{
	return true;
    }
}

function updateNavigationStatus(){
    var next = currentPage == pageCount - 1;
    var prev = currentPage == 0;
    var config = getConfig();

    if (currentPage == 0 && !(config.ssid.length > 0)){
	next = true;
	prev = true;
    }else if (currentPage == 1 && !(config.name.length > 0)){
	next = true;
	prev = true;
    }else if (currentPage == 2){
	var alen = config.password.length;
	var blen = config.passwordAgain.length;
	if ((alen > 0 && blen == 0) || (alen == 0 && blen > 0)){
	    next = true;
	    prev = true;
	}
    }

    $('#next').attr('disabled', next);
    $('#prev').attr('disabled', prev);
}

function getConfig(){
    return {
	"ssid" : $('input[name="ssid"]').val(),
	"ssidPass" : $('input[name="ssidPass"]').val(),
	"name" : $('input[name="deviceName"]').val(),
	"password" : $('input[name="adminPass"]').val(),
	"passwordAgain" : $('input[name="adminPassAgain"]').val()
    };
}

function commitConfig(){
    var config = getConfig();
    var reqBody = {
	"NodeName" : config.name,
	"SSID" : config.ssid,
	"WiFiPassword" : config.ssidPass,
	"commit" : true
    };
    if (config.password.length > 0){
	reqBody["AdminPassword"] = config.password;
    }
    
    $.ajax({
	type: 'POST',
	url: '/manage/config',
	data: JSON.stringify(reqBody),
	dataType: 'text',
	contentType: 'application/json',
	success: function(response){
	    $('#endWizardCover').attr('deactive', null);
	},
	error: function(req, err){
	    window.alert("Changing configuration failed.");
	}
    });
}