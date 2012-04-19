#!/usr/bin/env ruby

`makeinfo --html yafc.texi`
`cp yafc.css yafc`

def convert(filename)
  f = open(filename, "r");
  fname = filename.gsub(".html", ".php")
  ff = open(fname, "w");

  printf(ff, "<?php
 $title = 'manual :: %s';
 $desc = 'yet another ftp client';
 require('../header.php');
?>\n", File.basename(fname).gsub(".php", "").gsub("-", " "));

  in_body = false
 
  while !f.eof
	 l = f.readline
	 in_body = false if l =~ /<\/body>/
	 printf(ff, "%s", l.gsub(".html", ".php")) if in_body
	 in_body = true if l =~ /<body>/
  end

  printf(ff, "<?php require('../footer.php');?>\n");
	 
  f.close
  ff.close
  File.unlink(filename)
end

dir = Dir.open("yafc/")
begin
  dir.each {|name|
	 if name =~ /\.html$/
		convert("yafc/" + name);
	 end
  }
ensure
  dir.close
end
