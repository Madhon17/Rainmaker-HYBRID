<?php
require 'connect.php';

header('Content-Type: application/json; charset=utf-8');

try {
  $st = $pdo->query("SELECT r.uid, c.name, c.division, r.action, r.relays, r.created_at
                     FROM rfid_logs r
                     LEFT JOIN cards c ON r.uid = c.uid
                     ORDER BY r.id DESC LIMIT 100");
  $rows = $st->fetchAll(PDO::FETCH_ASSOC);
  echo json_encode(["data"=>$rows]);
} catch (Throwable $e) {
  echo json_encode(["data"=>[], "error"=>$e->getMessage()]);
}
