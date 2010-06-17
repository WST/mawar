SET NAMES UTF8;

-- таблица пользователей
CREATE TABLE users (
	id_user INT PRIMARY KEY auto_increment, -- ID пользователя
	user_login VARCHAR(40) UNIQUE COLLATE utf8_general_ci, -- логин пользователя (сравнение без учета регистра)
	user_password VARCHAR(80) -- пароль пользователя
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_general_cs;

-- Таблица оффлайн-сообщений
CREATE TABLE spool (
	id_message INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, -- ID сообщения
	message_to VARCHAR(255) NOT NULL, -- JID получателя
	message_stanza TEXT NOT NULL, -- Станза
	message_when INT UNSIGNED NOT NULL -- время
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_general_cs;

-- Приватное XML-хранилище
CREATE TABLE private_storage (
	id_block INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, -- ID блока
	username VARCHAR(40) NOT NULL, -- логин владельца блока
	block_xmlns VARCHAR(40) NOT NULL UNIQUE, -- xmlns блока для хранения
	block_data TEXT NOT NULL -- XML
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_general_cs;

CREATE TABLE roster (
	id_contact INT(11) UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
	id_user INT(11) UNSIGNED NOT NULL,
	contact_jid VARCHAR(64) NOT NULL,
	contact_nick VARCHAR(64) NOT NULL,
	contact_group VARCHAR(64) NOT NULL, -- для обозначения отсутствия группы будет использована пустая строка
	contact_subscription CHAR(1) NOT NULL,
	contact_pending CHAR(1) NOT NULL,
	UNIQUE INDEX contact (id_user, contact_jid)
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_general_cs;

CREATE TABLE vcard (
	id_vcard INT(11) UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
	id_user INT(11) UNSIGNED NOT NULL UNIQUE,
	vcard_data TEXT NOT NULL
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_general_cs;

-- пул s2s-станз ожидающих s2s-соединения
CREATE TABLE s2s_spool (
	hostname VARCHAR(80), -- имя хоста-получателя
	stanza TEXT NOT NULL, -- тело станхзы
	expires INT UNSIGNED NOT NULL, -- время устаревания станзы
	
	INDEX hostname (hostname), -- выборка записей по хосту
	INDEX expires (expires) -- выборка записей по дате устаревания
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_general_cs;

-- пул RFC 3921 (5.1.4) Directed Presence (#0000000963)
CREATE TABLE dp_spool (
	user_jid CHAR(120) NOT NULL, -- пользователь отправивший Directed Presence
	contact_jid CHAR(120) NOT NULL, -- контакт которому был отправлен Directed Presence
	PRIMARY KEY presence (user_jid, contact_jid)
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_general_cs;

