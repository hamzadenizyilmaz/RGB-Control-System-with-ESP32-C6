<?php
require_once 'connect.php';

$database = new Database();
$db = $database->getConnection();

if (!isset($_SERVER['HTTP_X_API_KEY']) || $_SERVER['HTTP_X_API_KEY'] != API_KEY) {
    http_response_code(401);
    echo json_encode(["message" => "Unauthorized"]);
    exit;
}

$method = $_SERVER['REQUEST_METHOD'];

if ($method == 'POST') {
    $data = json_decode(file_get_contents("php://input"));
    $state = $data->state ?? 0;

    $stmt = $db->prepare("INSERT INTO relay_states (state) VALUES (?)");
    $stmt->bind_param("i", $state);
    $stmt->execute();

    http_response_code(200);
    echo json_encode(["state" => $state]);
} elseif ($method == 'GET') {
    $result = $db->query("SELECT state FROM relay_states ORDER BY id DESC LIMIT 1");
    
    if ($result->num_rows > 0) {
        $row = $result->fetch_assoc();
        echo json_encode(["state" => $row['state']]);
    } else {
        echo json_encode(["state" => 0]);
    }
}
?>