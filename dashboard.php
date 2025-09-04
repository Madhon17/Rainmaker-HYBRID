<?php
// dashboard.php - versi AdminLTE
require 'connect.php';

// --- Helper ---
function safeQuery($pdo, $sql, $fallbackSql = null) {
  try {
    $st = $pdo->query($sql);
    return $st ? $st->fetchAll(PDO::FETCH_ASSOC) : [];
  } catch (Throwable $e) {
    if ($fallbackSql) {
      try {
        $st = $pdo->query($fallbackSql);
        return $st ? $st->fetchAll(PDO::FETCH_ASSOC) : [];
      } catch (Throwable $e2) {
        return ['__error' => $e->getMessage()];
      }
    }
    return ['__error' => $e->getMessage()];
  }
}

// Data Cards
$cards = safeQuery(
  $pdo,
  "SELECT uid, name, division, mask FROM cards ORDER BY uid ASC",
  "SELECT uid, '' AS name, '' AS division, mask FROM cards ORDER BY uid ASC"
);

// Data Logs (initial load, ajax nanti)
$logs = safeQuery(
  $pdo,
  "SELECT r.uid, c.name, c.division, r.action, r.relays, r.created_at
   FROM rfid_logs r
   LEFT JOIN cards c ON r.uid = c.uid
   ORDER BY r.id DESC
   LIMIT 20",
  "SELECT uid, '' AS name, '' AS division, action, relays
   FROM rfid_logs ORDER BY uid DESC LIMIT 20"
);

// Summary
$totalCards = is_array($cards) && !isset($cards['__error']) ? count($cards) : 0;
$totalLogs  = is_array($logs) && !isset($logs['__error']) ? count($logs) : 0;
?>
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="utf-8">
  <title>Dashboard Akses Kontrol</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <!-- AdminLTE + Bootstrap -->
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/admin-lte@3.2/dist/css/adminlte.min.css">
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap@4.6.2/dist/css/bootstrap.min.css">
  <link rel="stylesheet" href="https://cdn.datatables.net/1.13.7/css/dataTables.bootstrap4.min.css">

  <!-- Font Awesome -->
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css">

  <script src="https://code.jquery.com/jquery-3.6.0.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/bootstrap@4.6.2/dist/js/bootstrap.bundle.min.js"></script>
  <script src="https://cdn.datatables.net/1.13.7/js/jquery.dataTables.min.js"></script>
  <script src="https://cdn.datatables.net/1.13.7/js/dataTables.bootstrap4.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/admin-lte@3.2/dist/js/adminlte.min.js"></script>
</head>
<body class="hold-transition sidebar-mini layout-fixed">
<div class="wrapper">

  <!-- Navbar -->
  <nav class="main-header navbar navbar-expand navbar-white navbar-light">
    <ul class="navbar-nav">
      <li class="nav-item">
        <a class="nav-link" data-widget="pushmenu" href="#"><i class="fas fa-bars"></i></a>
      </li>
      <li class="nav-item d-none d-sm-inline-block"><a href="#" class="nav-link">Dashboard</a></li>
    </ul>
  </nav>

  <!-- Sidebar -->
  <aside class="main-sidebar sidebar-dark-primary elevation-4">
    <a href="#" class="brand-link">
      <span class="brand-text font-weight-light">Access Control</span>
    </a>
    <div class="sidebar">
      <nav class="mt-2">
        <ul class="nav nav-pills nav-sidebar flex-column">
          <li class="nav-item"><a href="#" class="nav-link active"><i class="nav-icon fas fa-id-card"></i><p>Cards</p></a></li>
          <li class="nav-item"><a href="#" class="nav-link"><i class="nav-icon fas fa-list"></i><p>Logs</p></a></li>
        </ul>
      </nav>
    </div>
  </aside>

  <!-- Content -->
  <div class="content-wrapper p-3">
    <div class="container-fluid">

      <!-- Summary cards -->
      <div class="row">
        <div class="col-lg-3 col-6">
          <div class="small-box bg-info">
            <div class="inner">
              <h3><?= $totalCards ?></h3>
              <p>Total Cards</p>
            </div>
            <div class="icon"><i class="fas fa-id-badge"></i></div>
          </div>
        </div>
        <div class="col-lg-3 col-6">
          <div class="small-box bg-success">
            <div class="inner">
              <h3><?= $totalLogs ?></h3>
              <p>Recent Logs</p>
            </div>
            <div class="icon"><i class="fas fa-clipboard-list"></i></div>
          </div>
        </div>
      </div>

      <!-- Cards Table -->
      <div class="card card-primary card-outline mb-4">
        <div class="card-header"><h3 class="card-title">Daftar Kartu</h3></div>
        <div class="card-body">
          <form class="form-inline mb-3" method="post" action="add_card.php">
            <input type="text" class="form-control mr-2" name="uid" placeholder="UID" required>
            <input type="text" class="form-control mr-2" name="name" placeholder="Nama" required>
            <input type="text" class="form-control mr-2" name="division" placeholder="Divisi" required>
            <input type="number" class="form-control mr-2" name="mask" placeholder="Mask (0-255)" required>
            <button type="submit" class="btn btn-success"><i class="fas fa-plus"></i> Tambah</button>
          </form>

          <table id="cardsTable" class="table table-bordered table-striped">
            <thead>
              <tr>
                <th>#</th>
                <th>UID</th>
                <th>Nama</th>
                <th>Divisi</th>
                <th>Mask</th>
                <th>Aksi</th>
              </tr>
            </thead>
            <tbody>
              <?php if (is_array($cards) && !isset($cards['__error'])): ?>
                <?php $no=1; foreach ($cards as $c): ?>
                  <tr>
                    <td><?= $no++ ?></td>
                    <td><?= htmlspecialchars($c['uid'] ?? '-') ?></td>
                    <td><?= htmlspecialchars($c['name'] ?? '-') ?></td>
                    <td><?= htmlspecialchars($c['division'] ?? '-') ?></td>
                    <td><span class="badge badge-info"><?= htmlspecialchars($c['mask'] ?? '-') ?></span></td>
                    <td>
                      <form method="post" action="remove_card.php" class="d-inline">
                        <input type="hidden" name="uid" value="<?= htmlspecialchars($c['uid'] ?? '') ?>">
                        <button type="submit" class="btn btn-danger btn-sm"><i class="fas fa-trash"></i> Hapus</button>
                      </form>
                    </td>
                  </tr>
                <?php endforeach; ?>
              <?php endif; ?>
            </tbody>
          </table>
        </div>
      </div>

      <!-- Logs Table -->
      <div class="card card-secondary card-outline">
        <div class="card-header"><h3 class="card-title">Log Akses</h3></div>
        <div class="card-body">
          <table id="logsTable" class="table table-bordered table-striped">
            <thead>
              <tr>
                <th>#</th>
                <th>UID</th>
                <th>Nama</th>
                <th>Divisi</th>
                <th>Action</th>
                <th>Relays</th>
                <th>Waktu</th>
              </tr>
            </thead>
            <tbody>
              <?php $no=1; foreach ($logs as $l): ?>
                <tr>
                  <td><?= $no++ ?></td>
                  <td><?= htmlspecialchars($l['uid'] ?? '-') ?></td>
                  <td><?= htmlspecialchars($l['name'] ?? '-') ?></td>
                  <td><?= htmlspecialchars($l['division'] ?? '-') ?></td>
                  <td>
                    <?php if (($l['action'] ?? '') === 'GRANTED'): ?>
                      <span class="badge badge-success">Granted</span>
                    <?php elseif (($l['action'] ?? '') === 'DENIED'): ?>
                      <span class="badge badge-danger">Denied</span>
                    <?php else: ?>
                      <span class="badge badge-secondary"><?= htmlspecialchars($l['action'] ?? '-') ?></span>
                    <?php endif; ?>
                  </td>
                  <td><?= htmlspecialchars($l['relays'] ?? '-') ?></td>
                  <td><?= htmlspecialchars($l['created_at'] ?? '-') ?></td>
                </tr>
              <?php endforeach; ?>
            </tbody>
          </table>
        </div>
      </div>

    </div>
  </div>
</div>

<script>
$(function () {
  $('#cardsTable').DataTable();

  // init logsTable
  let logsTable = $('#logsTable').DataTable({
    ajax: 'get_logs.php',
    columns: [
      { data: null, render: (d,t,r,m)=> m.row+1 },
      { data: 'uid' },
      { data: 'name' },
      { data: 'division' },
      { data: 'action', render: d => {
          if (d === 'GRANTED') return '<span class="badge badge-success">Granted</span>';
          if (d === 'DENIED') return '<span class="badge badge-danger">Denied</span>';
          return '<span class="badge badge-secondary">'+d+'</span>';
        }
      },
      { data: 'relays' },
      { data: 'created_at' }
    ],
    order: [[6,'desc']],
    pageLength: 10
  });

  // 🔄 auto refresh setiap 5 detik
  setInterval(() => {
    logsTable.ajax.reload(null, false); // false = biar tidak reset pagination
  }, 5000);
});
</script>

</body>
</html>
