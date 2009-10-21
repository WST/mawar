
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

-- Таблица закладок на MUC
CREATE TABLE bookmarks (
	id_bookmark INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, -- ID закладки
	id_user INT UNSIGNED NOT NULL, -- ID владельца закладки
	bookmark_name VARCHAR(40) NOT NULL, -- название закладки
	bookmark_jid VARCHAR(255) NOT NULL, -- JID закладки
	bookmark_nick VARCHAR(40) NOT NULL -- ник юзера
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_bin;
