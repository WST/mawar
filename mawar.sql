
-- таблица пользователей
CREATE TABLE users
(
	-- ID пользователя
	user_id INT PRIMARY KEY auto_increment,
	
	-- логин пользователя (сравнение без учета регистра)
	user_login VARCHAR(40) UNIQUE COLLATE utf8_general_ci,
	
	-- пароль пользователя
	user_password VARCHAR(80)
) DEFAULT CHARACTER SET UTF8 COLLATE utf8_bin;
