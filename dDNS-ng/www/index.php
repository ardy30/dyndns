<?php
	session_start();
	include_once './inc/config.php';
  require('./functions.php');
	include(SMARTY_LIB);

	$dsn = 'mysql:host='.DB_HOST.';dbname='.DBNAME;
	$dbh = new PDO($dsn, LOGIN, PASS);
	$www = new Smarty;
  $func = new Toolkit;
	$domains = array();
	$userdata = array();
	$newuser = array();
	$history = array();
	$user = array();
	$i = 0;
	$can_go = 0;

	if(isset($_POST['send'])) {
		if($_POST['login'] == NULL || $_POST['pass'] == NULL)
			$www->assign('error', "Incorrect data");
		else {
			$q = $dbh->query("SELECT login, pass FROM users WHERE login = '".$_POST['login']."' AND active = 1");
			$q->execute();
			if($q->rowCount() == 0)
				$www->assign('loginError', "Unknown user / Account inactive");
			else {
				$res = $q->fetch();
                if($func->checkPass($res['pass'], $_POST['pass']))
					$_SESSION['userlogin'] = $_POST['login'];
                else
					$www->assign('loginError', "Password incorrect");
			}
		}
	}
	if(isset($_POST['register'])) {
		if(!empty($_POST['name'])) {
			 $newuser['name'] = $_POST['name'];
			 $can_go++;
		}
		else
			$www->assign('error', "Empty Name");
		if(!empty($_POST['login'])) {
			 $newuser['login'] = $_POST['login'];
			 $can_go++;
		}
		else
			$www->assign('error', "Empty Login");
		if(!empty($_POST['pass'])) {
			$newuser['password'] = $_POST['pass'];
			$can_go++;
		}
		else
			$www->assign('error', "Empty password");
		if(!empty($_POST['conf_pass'])) {
			$newuser['confirm'] = $_POST['conf_pass'];
			$can_go++;
		}
		else
			$www->assign('error', "Empty confirm");
		if(!empty($_POST['email'])) {
			$newuser['email'] = $_POST['email'];
			$can_go++;
		}
		else
			$www->assign('error', "Empty email");
		$newuser['subdomain'] = $_POST['subdomain'];
		$newuser['domain']    = $_POST['domain'];
		if($newuser['password'] == $newuser['confirm']) {
			$newuser['pass_hash'] = crypt($newuser['password'], "$1$" . base64_encode(mcrypt_create_iv(6)));
			$can_go++;
		}
		else
			$www->assign('error', 'Passwords doesn\'t match');
		if($can_go > 5) {
			$activate_code = $func->genToken(24);
			$clientDomain = $_POST['subdomain'] . '.' . $_POST['domain'];
			$q = $dbh->prepare("SELECT count(*) as count FROM subdomains s, domains d ".
				"WHERE subdomain ='".$_POST['subdomain']."' and d.domain = '".$_POST['domain']."' and s.domain_id = d.id");
			$q->execute();
			$res = $q->fetch();
			if($res['count'] > 0) {
			   header('X-Message: Subdomain not available', true, 406);
			}
			else {
				$q = $dbh->prepare(
					"INSERT INTO users(login,pass,role,active,name,email,activate) VALUES ('".
					$newuser['login']."', '".$newuser['pass_hash']."', 'user', 0, '".$newuser['name']."', '".
					$newuser['email']."', '".$activate_code."')"
				);
				$q->execute();
				$q = $dbh->prepare(
					"SELECT users.id as user_id, domains.id as domain_id FROM users, domains WHERE users.login = '".$newuser['login'].
					"' AND domains.domain = '".$newuser['domain']."'"
				);
				$q->execute();
				$res = $q->fetch();
				$q = $dbh->prepare("INSERT INTO subdomains (user_id,domain_id,subdomain) VALUES(".$res['user_id'].", ".$res['domain_id'].", '".$newuser['subdomain']."')");
				$q->execute();
				$to = $newuser['email'];
				$message = "Hello " . $newuser['name'] . "!\r\n\r\n" .
							"To activate your dDNS account please click link below:\r\n".
							"http://".$_SERVER['HTTP_HOST']."/confirm.php?act=".$activate_code."\r\n\r\n".
							"--\r\n".
							"dDNS service";
				$func->notify($to, "Account confirmation", $message);
				$q = $dbh->prepare("SELECT email,name FROM users WHERE role = 'admin'");
				$q->execute();
				while($res = $q->fetch()) {
					$message = "Hello " . $res['name'] . "!\r\n\r\n" .
								"New user registered an account!\r\n\r\n".
								"Full name: ".$newuser['name']. "\r\n".
								"Login: " .$newuser['login']. "\r\n".
								"Subdomain: ". $newuser['subdomain']. ".".$newuser['domain']."\r\n\r\n".
								"--\r\n".
								"dDNS service";
					$func->notify($res['email'], "New account", $message);
				}
			}
		}
		else
             header('X-Message: Incomplete data', true, 406);
	}
	if($_GET['opt'] == 'logout') {
		unset($_SESSION['userlogin']);
		header("Location: /");
	}

	if(isset($_SESSION['userlogin'])) {
		$q = $dbh->prepare("SELECT u.id, u.name,u.login, u.email, u.role, u.active, ip, lastupdate, ".
		"CONCAT(s.subdomain, \".\",  d.domain) as subdomain ".
		"FROM subdomains s, domains d , users u ".
		"WHERE s.domain_id = d.id and u.id = s.user_id and u.login = '".$_SESSION['userlogin']."'");
		$q->execute();
		if($q->rowCount() == 0)
			$www->assign('error', "No data");
		else {
			$res = $q->fetch();
			$userdata['login'] = $res['login'];
			$userdata['name'] = $res['name'];
			$userdata['role'] = $res['role'];
			$userdata['email'] = $res['email'];
			$userdata['subdomain'] = $res['subdomain'];
			$userdata['ip'] = $res['ip'];
			$userdata['lastupdate'] = substr($res['lastupdate'], 1, -1);

		}
		$www->assign('userdata', $userdata);
		$q = $dbh->prepare(
		"SELECT * FROM user_log WHERE user_id = (SELECT id from users WHERE login = '".$_SESSION['userlogin']."') ORDER BY id DESC");
		$q->execute();
		if($q->rowCount() == 0)
			   $www->assign('error', "No data");
		else {
			   while($res = $q->fetch()) {
					   $history[substr($res['date'], 1, -1)] = $res['ip'];
			   }
			   $www->assign('history', $history);
		}
		$q = $dbh->prepare("SELECT u.id, u.name,u.login, u.email, u.role, u.active, ip, lastupdate, ".
		"CONCAT(s.subdomain, \".\",  d.domain) as subdomain ".
		"FROM subdomains s, domains d , users u ".
		"WHERE s.domain_id = d.id and u.id = s.user_id AND s.dynamic = 1");
		$q->execute();
		if($q->rowCount() > 0) {
		$i = 0;
		while($res = $q->fetch())
			$user[$i++] = array(
				$res['id'], $res['name'], $res['login'], $res['email'],
				$res['role'], $res['active'], $res['subdomain']
			);
		}
		$www->assign('allusers', $user);
		if(isset($_POST['changepass'])) {
		   $q = $dbh->prepare("SELECT pass FROM users WHERE login = '".$_SESSION['userlogin']."'");
		   $q->execute();
		   if($q->rowCount() > 0) {
			  $res = $q->fetch();
			  if($func->checkPass($res['pass'], $_POST['oldpass'])) {
				$salt = substr($res['pass'], 0, 12);
				$passwordHash = crypt($_POST['newpass'], $salt);
				$q = $dbh->prepare("UPDATE users SET pass = '".$passwordHash."' WHERE login = '".$_SESSION['userlogin']."'");
				$q->execute();
			  }
			  else
				   header('X-Message: Old password incorrect', true, 406);
		   }
		}
		if($_POST['update'] == 'user') {
			$q = $dbh->query("UPDATE users SET name = '".$_POST['name']."', email = '".$_POST['email']."', subdomain = '".$_POST['subdomain']."' WHERE login = '".$_SESSION['userlogin']."'");
			$status = $q->execute();
		}
	}
	$www->assign('title', 'dDNS Service');
	$www->assign('desc', 'Dynamic DNS system');
	foreach ($dbh->query("SELECT domain FROM domains WHERE status = 'public'") as $row) {
		 $domains[$i] = $row['domain'];
		 $i++;
	}
	$www->assign('domains', $domains);
	$www->assign('page', $_GET['opt']);
	$www->display('test.tpl');
?>
