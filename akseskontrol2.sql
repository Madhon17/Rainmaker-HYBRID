-- phpMyAdmin SQL Dump
-- version 5.2.1
-- https://www.phpmyadmin.net/
--
-- Host: 127.0.0.1
-- Generation Time: Sep 04, 2025 at 07:26 AM
-- Server version: 10.4.32-MariaDB
-- PHP Version: 8.2.12

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `akseskontrol2`
--

-- --------------------------------------------------------

--
-- Table structure for table `cards`
--

CREATE TABLE `cards` (
  `uid` varchar(32) NOT NULL,
  `name` varchar(100) NOT NULL,
  `division` varchar(100) NOT NULL,
  `mask` tinyint(3) UNSIGNED NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `cards`
--

INSERT INTO `cards` (`uid`, `name`, `division`, `mask`) VALUES
('AA119F88', 'Siti Aminah', 'HRD', 5),
('BB722E06', 'Budi Santoso', 'IT Support', 3);

-- --------------------------------------------------------

--
-- Table structure for table `rfid_logs`
--

CREATE TABLE `rfid_logs` (
  `id` int(11) NOT NULL,
  `uid` varchar(32) NOT NULL,
  `action` varchar(20) NOT NULL,
  `relays` varchar(50) DEFAULT '-',
  `created_at` timestamp NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `rfid_logs`
--

INSERT INTO `rfid_logs` (`id`, `uid`, `action`, `relays`, `created_at`) VALUES
(27, '96D56D05', 'GRANTED', 'R6,R7,R8', '2025-09-04 05:02:23'),
(28, 'BB722E06', 'GRANTED', 'R8', '2025-09-04 05:02:32'),
(29, '96D56D05', 'GRANTED', 'R6,R7,R8', '2025-09-04 05:18:01'),
(30, 'BB722E06', 'GRANTED', 'R8', '2025-09-04 05:18:34'),
(31, 'BB722E06', 'GRANTED', 'R8', '2025-09-04 05:18:48'),
(32, 'BB722E06', 'GRANTED', 'R8', '2025-09-04 05:19:13'),
(33, '96D56D05', 'GRANTED', 'R6,R7,R8', '2025-09-04 05:22:31'),
(34, '96D56D05', 'GRANTED', 'R6,R7,R8', '2025-09-04 05:22:38'),
(35, 'BB722E06', 'GRANTED', 'R8', '2025-09-04 05:22:45'),
(36, '045825A5E61990', 'GRANTED', 'R1,R2,R3,R4,R5,R6,R7,R8', '2025-09-04 05:24:18'),
(37, '045825A5E61990', 'REMOVE', '-', '2025-09-04 05:24:52'),
(38, '045825A5E61990', 'REMOVE', '-', '2025-09-04 05:24:52'),
(39, '045825A5E61990', 'DENIED', '-', '2025-09-04 05:24:58');

--
-- Indexes for dumped tables
--

--
-- Indexes for table `cards`
--
ALTER TABLE `cards`
  ADD PRIMARY KEY (`uid`);

--
-- Indexes for table `rfid_logs`
--
ALTER TABLE `rfid_logs`
  ADD PRIMARY KEY (`id`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `rfid_logs`
--
ALTER TABLE `rfid_logs`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=40;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
