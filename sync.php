<?php
require 'connect.php';

$stmt = $pdo->query("SELECT uid, mask, updated_at FROM cards");
$cards = $stmt->fetchAll();
echo json_encode(["ok"=>true,"cards"=>$cards]);
?>
