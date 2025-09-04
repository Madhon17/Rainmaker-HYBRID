<?php
$host = 'localhost';
$db   = 'akseskontrol2';
$user = 'root';
$pass = '';
$charset = 'utf8mb4';

$dsn = "mysql:host=$host;dbname=$db;charset=$charset";
$options = [
    PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
    PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
];
$pdo = new PDO($dsn, $user, $pass, $options);

// Optional: API key check
$API_KEY = "mysecret123";
if (isset($_GET['api_key']) || isset($_POST['api_key'])) {
    $key = $_GET['api_key'] ?? $_POST['api_key'];
    if ($key !== $API_KEY) {
        http_response_code(403);
        die(json_encode(["ok"=>false,"err"=>"Invalid API key"]));
    }
}
?>
