<?xml version="1.0" ?>
<stream:stream
	xmlns:stream="http://etherx.jabber.org/streams"
	version="1.0" xmlns="jabber:server" to="$1" xml:lang="ru"
	xmlns:xml="http://www.w3.org/XML/1998/namespace">
<?php

class parser
{
	/**
	* SAX парсер
	*/
	protected $parser;
	
	/**
	* Атрибуты тегов
	*/
	protected $attrs;
	
	/**
	* Буфер для накопления текстовых данных
	* @see catchText() и getCatchedText()
	*/
	protected $text;
	
	/**
	* Признак захвата текста
	* @see catchText() и getCatchedText()
	*/
	protected $catch_text;
	
	protected $iq_id;
	protected $iq_from;
	
	/**
	* Конструктор SAX-парсера
	*
	* @note Если потомок переопределяет конструктор, то он обязан вызывать
	* родительский конструктор: parent::__construct();
	*/
	public function __construct()
	{
		$this->parser = xml_parser_create();
		xml_set_object($this->parser, $this);
		xml_set_element_handler($this->parser, "on_start_element", "on_end_element");
		xml_set_character_data_handler($this->parser, "on_cdata");
		xml_parser_set_option($this->parser, XML_OPTION_CASE_FOLDING, 0);
		xml_parser_set_option($this->parser, XML_OPTION_SKIP_WHITE, 0);
		xml_parser_set_option($this->parser, XML_OPTION_TARGET_ENCODING, 'UTF-8');
	}
	
	/**
	* Деструктор парсера
	*
	* @note Если потомок переопределяет деструктор, то желательно, чтобы
	* потомк вызывал родительский: parent::__destruct()
	*/
	public function __destruct()
	{
		xml_parser_free($this->parser);
	}
	
	/**
	* Сброс парсера на начальные настройки
	*/
	public function reset()
	{
		xml_parser_free($this->parser);
		$this->parser = xml_parser_create();
		xml_set_object($this->parser, $this);
		xml_set_element_handler($this->parser, "on_start_element", "on_end_element");
		xml_set_character_data_handler($this->parser, "on_cdata");
		xml_parser_set_option($this->parser, XML_OPTION_CASE_FOLDING, 0);
		xml_parser_set_option($this->parser, XML_OPTION_SKIP_WHITE, 0);
		xml_parser_set_option($this->parser, XML_OPTION_TARGET_ENCODING, 'UTF-8');
	}
	
	/**
	* Парсинг очередного куска XML-файла
	* @param string кусок XML-файла для парсига
	* @param bool признак последнего куска
	* @return bool TRUE - успешно, FALSE - произошла ошибка
	*/
	public function parse($data, $isFinal = true)
	{
		return xml_parse($this->parser, $data, $isFinal);
	}
	
	/**
	* Вернуть код ошибки
	* @return bool код ошибки
	*/
	public function errno()
	{
		return xml_get_error_code($this->parser);
	}
	
	/**
	* Вернуть сообщение об ошибке
	* @return string сообщение об ошибке
	*/
	public function error()
	{
		return xml_error_string(xml_get_error_code($this->parser));
	}
	
	/**
	* Начать захват текста
	*/
	protected function catchText()
	{
		$this->catch_text = true;
		$this->text = '';
	}
	
	/**
	* Закончить захват текста
	* @return string захваченый текст
	*/
	protected function getCatchedText()
	{
		if ( $this->catch_text )
		{
			$result = $this->text;
			$this->text = null;
			$this->catch_text = false;
			return $result;
		}
		return null;
	}
	
	/**
	* Обработка символьных данных
	* @param resource парсер
	* @param string текстовые данные
	*/
	protected function on_cdata($parser, $data)
	{
		if ( $this->catch_text )
		{
			$this->text .= $data;
		}
	}
	
	/**
	* Обработка открывающегося тега
	* @param resource парсер
	* @param string имя открывающегося тега
	* @param array атрибуты тега
	*/
	protected function on_start_element($parser, $name, $attrs)
	{
		if ( method_exists($this, "catch_{$name}_tag") )
		{
			$this->attrs = $attrs;
			$this->catchText();
		}
		elseif ( method_exists($this, $method = "start_{$name}_tag") )
		{
			call_user_func(array(& $this, $method), $attrs);
		}
		else
		{
			$this->start_element($name, $attrs);
		}
	}
	
	/**
	* Обработка закрывающегося тега
	* @param resource парсер
	* @param string имя закрывающегося тега
	*/
	protected function on_end_element($parser, $name)
	{
		if ( method_exists($this, $method = "catch_{$name}_tag") )
		{
			$text = $this->getCatchedText();
			call_user_func(array(& $this, $method), $text, $this->attrs);
		}
		elseif ( method_exists($this, $method = "end_{$name}_tag") )
		{
			call_user_func(array(& $this, $method));
		}
		else
		{
			$this->end_element($name);
		}
	}
	
	protected function start_iq_tag($attrs)
	{
		$this->iq_id = isset($attrs['id']) ? $attrs['id'] : '';
		$this->iq_from = $attrs['from'];
	}
	
	protected function end_iq_tag()
	{
		$id = htmlspecialchars($this->iq_id);
		$to = htmlspecialchars($this->iq_from);
		echo "<!-- PHP -->\n";
		echo "<iq from='mkpnet.ru' to='$to' id='$id' type='result' />\n";
	}
	
	/**
	* Обработка открывающегося тега
	* @param string имя открывающегося тега
	* @param array атрибуты открывающегося тега
	*/
	protected function start_element($name, $attrs)
	{
	}
	
	/**
	* Обработка закрывающегося тега
	* @param string имя закрывающегося тега
	*/
	protected function end_element($name)
	{
	}
}

$parser = new parser();

while ( ($data = fread(STDIN, 4096)) !== false )
{
	if ( ! $parser->parse($data, false) )
	{
		echo $parser->error();
		break;
	}
}
$parser->parse("", true);

?>
</stream:stream>
