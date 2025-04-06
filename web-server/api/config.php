<?php

define('DB_HOST', 'localhost');
define('DB_USER', 'led');
define('DB_PASS', 'ledpas');
define('DB_NAME', 'led_control');

define('API_KEY', 'ESP32_SECRET_KEY');

header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json; charset=UTF-8");
header("Access-Control-Allow-Methods: GET, POST");
header("Access-Control-Max-Age: 3600");
header("Access-Control-Allow-Headers: Content-Type, Access-Control-Allow-Headers, Authorization, X-Requested-With");
?>