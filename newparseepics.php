<?php

$x = file_get_contents("php://stdin");


$p = preg_match("/object-identifier: \\(device, ([0-9]+)\\)/",$x,$matches);
if (isset($matches[1])) {
	$device = $matches[1];
} else {
	echo "device id not found.\n";
	exit(0);
}



// delete before data of interest
$x = preg_replace("/^.*?-- Found [0-9]+ Objects \n  }, \n/s","",$x);

// delete after data of interest
$x = preg_replace("/\r.*$/s","",$x);


// leading 8 spaces means continuation from previous line
$x = preg_replace("/\n        /",""    ,$x);

// punting on state-text
$x = preg_replace("/\n    state-text:.*?\n/s","\n",$x);

// delete BACnet error
$x = preg_replace("/\n(.*?)BACnet Error(.*?)\n/","\n",$x);

// replace proprietary
$x = preg_replace("/-- proprietary /","proprietary-",$x);

// slash quotes
$x = preg_replace("/\"/","\\\"",$x);

// for properties with no value, poke in a ""
$x = preg_replace("/\n    ([^:]*?): \n/","\n    \\1: \\\"\\\"\n",$x);

// place quote before property name
$x = preg_replace("/\n    /","\n    \"",$x);

// place quote between property name and colon (:)
$x = preg_replace("/: /","\": ",$x);

// place quotes around not-already-quoted values
$x = preg_replace("/: (.*?)\n/",": \"\\1\",\n", $x);

// place quotes around single character values that aren't a quote symbol
$x = preg_replace("/: ([^\"])\n/",": \"\\1\",\n", $x);

//print_r($x); exit(0);
// replace any quote followed by a newline with a quote+comma
$x = preg_replace("/\"\n/","\",\n",$x);

// delete any commas followed by \n space space close curly 
$x = preg_replace("/,\n  }/","\n  }",$x);

$x= "[ $x ]\n";

//echo $x; exit(0);

$j=json_decode($x);

//print_r($j);

/*
    -- proprietary 32527: "metasys3:S1-NAE07/N2-1.071009UN.GLRF_AC5 DA-T"
    -- proprietary 2390: "GLRF.AC-5.DA-T"
    object-name: "S1-NAE07/N2-1.071009UN.GLRF_AC5 DA-T"
    description: "Discharge Temperature"
 */




$objects = [];
echo "device_id,obj_type,obj_id,obj_name,obj_description,obj_name_2390,obj_name_32527\n";
foreach ($j as $obj) {
	$object_identifier = $obj->{'object-identifier'};
	$object_identifier = substr($object_identifier,1,-1);
	list($obj_type, $obj_id) = explode(", ",$object_identifier);
	if ($obj_type == "trend-log") continue;
	$obj_description = stripslashes($obj->{'description'});
	$obj_name = stripslashes($obj->{'object-name'});
	$obj_name_2390 = stripslashes($obj->{'proprietary-2390'});
	$obj_name_32527 = stripslashes($obj->{'proprietary-32527'});
	echo "$device,$obj_type,$obj_id,$obj_name,$obj_description,$obj_name_2390,$obj_name_32527\n";
}


exit(0);
