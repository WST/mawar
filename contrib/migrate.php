#!/usr/bin/php
<?php

define('MYSQL_ROOT_PASSWORD', 'password here');
define('DB_EJABBERD', 'ejabberd_underjabber');
define('DB_MAWAR', 'mawar_underjabber');

set_time_limit(0);

if(!function_exists('mysql_connect')) {
	die("MySQL functionality is not available!\n");
}

if(!$cnx =  mysql_connect('localhost', 'root', MYSQL_ROOT_PASSWORD)) {
	die(mysql_error());
}

mysql_query('SET NAMES utf8', $cnx);
mysql_query('SET character_set_client=UTF8', $cnx);

echo "Moving data has begun\n";

$sql = 'SELECT * FROM ' . DB_EJABBERD . '.users';
$u = mysql_query($sql, $cnx);
while($u_res = mysql_fetch_assoc($u)) {
	$sql = 'INSERT INTO ' . DB_MAWAR . '.users (user_login, user_password) VALUES (\'' . mysql_real_escape_string($u_res['username'], $cnx) . '\', \'' . mysql_real_escape_string($u_res['password'], $cnx) . '\')';
	mysql_query($sql, $cnx);
	$id_user = mysql_insert_id($cnx);
	
	$sql = 'SELECT u.jid, u.nick, u.subscription, u.ask, g.grp FROM ' . DB_EJABBERD . '.rosterusers AS u LEFT JOIN ' . DB_EJABBERD . '.rostergroups AS g ON (u.username = g.username AND u.jid = g.jid) WHERE u.username=\'' . mysql_real_escape_string($u_res['username'], $cnx) . '\'';
	$r = mysql_query($sql, $cnx);
	while($r_res = mysql_fetch_assoc($r)) {
		$sql = 'INSERT INTO ' . DB_MAWAR . '.roster (id_user, contact_jid, contact_nick, contact_group, contact_subscription, contact_pending) VALUES (' . $id_user . ', \'' . mysql_real_escape_string($r_res['jid'], $cnx) . '\', \'' . mysql_real_escape_string($r_res['nick'], $cnx) . '\', \'' . mysql_real_escape_string($r_res['grp'], $cnx) . '\', \'' . $r_res['subscription'] . '\', \'' . $r_res['ask'] . '\')';
		mysql_query($sql, $cnx);
	}
	
	// NOTE: в таблице private_storage была выполнена денормализация
	$sql = 'INSERT INTO ' . DB_MAWAR . '.private_storage (username, block_xmlns, block_data) SELECT username, namespace, data FROM ' . DB_EJABBERD . '.private_storage';
	mysql_query($sql, $cnx);
	
	$sql = 'SELECT vcard FROM ' . DB_EJABBERD . '.vcard WHERE username=\'' . mysql_real_escape_string($u_res['username'], $cnx) . '\'';
	$v = mysql_query($sql, $cnx);
	if($v_res = mysql_fetch_assoc($v)) {
		$sql = 'INSERT INTO ' . DB_MAWAR . '.vcard (id_user, vcard_data) VALUES (' . $id_user . ', \'' . mysql_real_escape_string($v_res['vcard'], $cnx) . '\')';
		mysql_query($sql, $cnx);
	}
	mysql_free_result($v);
	
	echo 'Done for user: ' . $u_res['username'] . " \n";
}
mysql_free_result($u);

@ mysql_close($cnx);

echo "Operation finished!\n";

?>
