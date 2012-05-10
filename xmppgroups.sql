SET NAMES UTF8;

-- таблица пользователей
CREATE TABLE xmpp_groups
(
	group_name VARCHAR(80) NOT NULL UNIQUE,
	group_title VARCHAR(255) NOT NULL DEFAULT '',
	group_info MEDIUMTEXT NOT NULL DEFAULT ''
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_bin;

CREATE TABLE group_subscribers
(
	group_name VARCHAR(80) NOT NULL,
	contact_name VARCHAR(80) NOT NULL,
	contact_jid VARCHAR(80) NOT NULL,
	contact_owner TINYINT(0) NOT NULL DEFAULT 0,
	
	PRIMARY KEY (group_name, contact_name),
	UNIQUE INDEX jid(group_name, contact_jid)
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_bin;
