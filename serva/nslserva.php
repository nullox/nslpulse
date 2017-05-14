<?php

/*

ABOUT:

Drops useful summary data concerning an active LAMP stack server. May be
utilised by an autonomous client to sweep across a cluster of servers to
pinpoint those approaching load capacities and maintain an eye on running
database processes.

Clients may thereafter be configured to dispatch alerts to admin personnel
to address these key infrastructural concerns.

Nullox Standard Library Component
Copyright 2017 (c) Nullox Software
Written by William Johnson <johnson@nullox.com>

LICENSE:

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the project nor the names of its contributors may be
used to endorse or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

DEPLOYMENT NOTES:

1. Change the PASSWD_HASH value. The hash is gost in this example but you may
   use whatever you like. You can even remove the authentication check but I'd
   highly recommend against it.

2. Finally and most importantly, rename this script before you deploy it in the
   wild to prevent autonomous landers.

3. Keep a secure note of all hashes you generate for this script and configure
   your client to point to the location of the server script(s).

Path example: http://IP_ADDR_HERE/nslserva.php?hKey=NSLPULSE_PUBLIC_DEMO

*/

define('PASSWD_HASH', '6ccfeba6a68928d2de83264c80b412161cd5e771be13a2d1f9411affdfb679cb');
define('HASHING_TYPE', 'gost');
define('DATA_DELIMITOR', ':');

$hKey = isset($_REQUEST['hKey']) ? $_REQUEST['hKey'] : '';

if ( strcmp(hash(HASHING_TYPE, $hKey), PASSWD_HASH) != 0 )
  exit;

/*   for windows boxes, WMIC is ALOT faster than
     COM interfaces provided by some .net library
     or systeminfo commands. Find the WMIC alternative
     and stick to it */
class NslServer
{
  /* binary interpretation of metric is used, stats relate to disk etc
     thus 2014 bytes per KB is used instead of the SI standard of 1,000 */
  const BYTES_PER_KB = 1024;

  /* process name list */
  const DB_PROCESSES = array('mysql', 'mysqld.exe', 'mysqld');

  /* returns percentile of load (mean across cores) */
  public static function CpuLoad() {
    /* sys_getloadavg doesn't exist on winos boxes */
    if ( !function_exists('sys_getloadavg') ) {
      function sys_getloadavg() {
        exec('wmic cpu get LoadPercentage', $p);
        return $p[1] / 100;
      }
      $load = sys_getloadavg();
      if ( is_array($load) )
        $load = $load[0];
      return $load;
    }
  }

  /* queries the running state of a database service, returns
     false if no expectant process is found */
  public static function DatabaseCheck() {
    if (strtoupper(substr(PHP_OS, 0, 3)) === 'WIN') {
      exec('tasklist', $p);
      foreach($p as $processEntry) {
        $processEntry = explode(' ', $processEntry);
        $processEntry = $processEntry[0];
        foreach(NslServer::DB_PROCESSES as $dbProcess) {
          if ( strcmp($processEntry, $dbProcess) == 0 )
            return true;
        }
      }
      return false;
    }
    else {
      foreach(NslServer::DB_PROCESSES as $dbProcess) {
        if ( is_numeric(trim(exec('pgrep '. $dbProcess))) )
          return true;
      }
      return false;
    }
  }

  /* returns array of working disk space in KB of
     the form: (totalSpace, freeSpace, usagePercent) */
  public static function DiskSpace() {
    $tds = (int)(disk_total_space('.') / NslServer::BYTES_PER_KB);
    $fds = (int)(disk_free_space('.') / NslServer::BYTES_PER_KB);
    return array($tds, $fds, 1.0-round($fds/$tds, 4));
  }

  /* returns the uptime in seconds */
  public static function Uptime() {
    if (strtoupper(substr(PHP_OS, 0, 3)) === 'WIN') {
      exec('wmic os get lastbootuptime', $p);
      $bt = explode('.', $p[1]);
      $bt = $bt[0];

      /* boot time to unix time */
      $btasut = mktime(substr($bt, 8, 2), /* hour */
                       substr($bt, 10, 2),/* minute */
                       substr($bt, 12, 2),/* second */
                       substr($bt, 4, 2), /* month */
                       substr($bt, 6, 2), /* day/date */
                       substr($bt, 0, 4));/* year */
      return time() - $btasut;
    }
    else {
      return exec("awk '{print $1}' /proc/uptime");
    }
  }

  /* returns array of working ram stats in KB of
     of the form: (totalMem, freeMem, usagePercent) */
  public static function WorkingRam() {
    $fpm = '';
    $tpm = '';
    if (strtoupper(substr(PHP_OS, 0, 3)) === 'WIN') {
      exec('wmic OS get FreePhysicalMemory /Value', $p);
      $p = array_slice(array_filter($p), 0);
      $fpm = (int)(str_replace('FreePhysicalMemory=', '', $p[0]));
      exec('wmic computersystem get TotalPhysicalMemory', $q);
      $tpm = (int)($q[1] / NslServer::BYTES_PER_KB);
    }
    else {
      $data = explode('\n', trim(file_get_contents("/proc/meminfo")));
      $meminfo = array();
      foreach ($data as $line) {
          list($key, $val) = explode(":", $line);
          $meminfo[$key] = trim($val);
      }
      $tpm = (int)($meminfo['MemTotal']);
      $fpm = (int)($meminfo['MemFree']);
    }
    return array($tpm, $fpm, 1.0-round($fpm/$tpm, 4));
  }
}

$cpuLoad = NslServer::CpuLoad();
$activeDb = NslServer::DatabaseCheck();
$diskSpace = NslServer::DiskSpace();
$upTime = NslServer::Uptime();
$workingRam = NslServer::WorkingRam();

$delimitString = $cpuLoad .DATA_DELIMITOR. $activeDb .DATA_DELIMITOR;
$delimitString .= $upTime .DATA_DELIMITOR. $diskSpace[0] .DATA_DELIMITOR;
$delimitString .= $diskSpace[1] .DATA_DELIMITOR. $diskSpace[2];
$delimitString .= DATA_DELIMITOR. $workingRam[0] .DATA_DELIMITOR;
$delimitString .= $workingRam[1] .DATA_DELIMITOR. $workingRam[2];

echo($delimitString);

?>
