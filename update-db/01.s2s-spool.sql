SET NAMES UTF8;

-- пул s2s-станз ожидающих s2s-соединения
CREATE TABLE s2s_spool (
	hostname VARCHAR(80), -- имя хоста-получателя
	stanza TEXT NOT NULL, -- тело станхзы
	expires INT UNSIGNED NOT NULL, -- время устаревания станзы
	
	INDEX hostname (hostname), -- выборка записей по хосту
	INDEX expires (expires) -- выборка записей по дате устаревания
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_bin;
