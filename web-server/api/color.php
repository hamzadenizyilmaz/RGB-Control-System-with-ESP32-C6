<?php
require_once 'connect.php';

$database = new Database();
$db = $database->getConnection();

// API Key kontrolü
if (!isset($_SERVER['HTTP_X_API_KEY']) || $_SERVER['HTTP_X_API_KEY'] != API_KEY) {
    http_response_code(401);
    echo json_encode(["message" => "Unauthorized"]);
    exit;
}

$method = $_SERVER['REQUEST_METHOD'];

if ($method == 'POST') {
    $data = json_decode(file_get_contents("php://input"));
    
    $red = $data->red ?? 0;
    $green = $data->green ?? 0;
    $blue = $data->blue ?? 0;
    $brightness = $data->brightness ?? 100;

    $stmt = $db->prepare("INSERT INTO color_settings (red, green, blue, brightness) VALUES (?, ?, ?, ?)");
    $stmt->bind_param("iiii", $red, $green, $blue, $brightness);
    $stmt->execute();

    http_response_code(200);
    echo json_encode([
        "red" => $red,
        "green" => $green,
        "blue" => $blue,
        "brightness" => $brightness
    ]);
} elseif ($method == 'GET') {
    $result = $db->query("SELECT * FROM color_settings ORDER BY id DESC LIMIT 1");
    
    if ($result->num_rows > 0) {
        $row = $result->fetch_assoc();
        echo json_encode($row);
    } else {
        echo json_encode([
            "red" => 255,
            "green" => 0,
            "blue" => 0,
            "brightness" => 100
        ]);
    }
}
?>