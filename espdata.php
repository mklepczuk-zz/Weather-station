<?php
include('config.php');
$w_api_key = $_GET['api_key'];
$table = $_GET['station_id'];
$t = $_GET['t'];
$h = $_GET['h'];
$ap = $_GET['ap'];
$rp = $_GET['rp'];
$li = $_GET['li'];
$vl = $_GET['vl'];

$conn = mysqli_connect($db_host, $db_user, $db_pass, $db_name);

if ($w_api_key == $write_api_key){

	$result = mysqli_query($conn,"INSERT INTO $table(temperature, humidity, absolute_pressure, relative_pressure, light_intensity, voltage_level) VALUES ($t,$h,$ap,$rp,$li,$vl)");
	if ($result === false){
		echo "ERR";
		$conn -> close();
	}
	else{
		echo "OK";
		$conn -> close();
	}
}else{
	echo "Niepoprawny API-KEY";
}
?>
