
-- таблица пользователей
CREATE TABLE users (
	id_user INT PRIMARY KEY auto_increment, -- ID пользователя
	user_login VARCHAR(40) UNIQUE COLLATE utf8_general_ci, -- логин пользователя (сравнение без учета регистра)
	user_password VARCHAR(80) -- пароль пользователя
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_bin;

-- Таблица оффлайн-сообщений
CREATE TABLE spool (
	id_message INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, -- ID сообщения
	message_to VARCHAR(255) NOT NULL, -- JID получателя
	message_stanza TEXT NOT NULL, -- Станза
	message_when INT UNSIGNED NOT NULL -- время
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_bin;

-- Приватное XML-хранилище
CREATE TABLE private_storage (
	id_block INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, -- ID блока
	id_user INT UNSIGNED NOT NULL, -- ID владельца блока
	block_xmlns VARCHAR(40) NOT NULL UNIQUE, -- xmlns блока для хранения
	block_data TEXT NOT NULL -- XML
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_bin;

CREATE TABLE roster (
	id_contact INT(11) UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
	id_user INT(11) UNSIGNED NOT NULL,
	contact_jid VARCHAR(64) NOT NULL,
	contact_nick VARCHAR(64) NOT NULL,
	contact_group VARCHAR(64) NOT NULL, -- для обозначения отсутствия группы будет использована пустая строка
	contact_subscription CHAR(1) NOT NULL,
	contact_pending CHAR(1) NOT NULL
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_bin;

CREATE TABLE vcard (
	id_vcard INT(11) UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
	id_user INT(11) UNSIGNED NOT NULL UNIQUE,
	vcard_data TEXT NOT NULL
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_bin;
