<?php
require 'connect.php';
$uid = $_POST['uid'] ?? '';
$action = $_POST['action'] ?? '';
$relays = $_POST['relays'] ?? '';

if ($uid && $action) {
    $stmt = $pdo->prepare("INSERT INTO rfid_logs (uid, action, relays) VALUES (?, ?, ?)");
    $stmt->execute([$uid, $action, $relays]);
    echo json_encode(["ok"=>true]);
} else {
    echo json_encode(["ok"=>false,"err"=>"invalid_data"]);
}
?>
