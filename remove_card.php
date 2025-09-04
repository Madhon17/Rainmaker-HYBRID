<?php
require 'connect.php';
$uid = $_POST['uid'] ?? '';
$updated = $_POST['updated_at'] ?? date('Y-m-d H:i:s');

if ($uid) {
    $pdo->prepare("DELETE FROM cards WHERE uid=?")->execute([$uid]);
    $pdo->prepare("INSERT INTO rfid_logs (uid, action) VALUES (?, 'REMOVE')")->execute([$uid]);
    echo json_encode(["ok"=>true]);
} else {
    echo json_encode(["ok"=>false,"err"=>"no_uid"]);
}
?>
