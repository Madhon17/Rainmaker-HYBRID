<?php
require 'connect.php';
$uid = $_POST['uid'] ?? '';
$mask = intval($_POST['mask'] ?? 0);
$updated = $_POST['updated_at'] ?? date('Y-m-d H:i:s');

if ($uid) {
    $stmt = $pdo->prepare("INSERT INTO cards (uid, mask, updated_at) 
                           VALUES (?, ?, ?) 
                           ON DUPLICATE KEY UPDATE mask=VALUES(mask), updated_at=VALUES(updated_at)");
    $stmt->execute([$uid, $mask, $updated]);
    $pdo->prepare("INSERT INTO rfid_logs (uid, action, relays) VALUES (?, 'ADD', ?)")
        ->execute([$uid, $mask]);
    echo json_encode(["ok"=>true]);
} else {
    echo json_encode(["ok"=>false,"err"=>"no_uid"]);
}
?>
