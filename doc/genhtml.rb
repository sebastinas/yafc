#!/usr/bin/env ruby

`makeinfo --html yafc.texi`
`cp yafc.css yafc`

def convert(filename)
  f = open(filename, "r");
  ff = open(filename + ".2", "w");

  printf(ff, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"
 \"http://www.w3.org/TR/html4/strict.dtd\">\n");

  while !f.eof
	 l = f.readline
	 if l =~ /link href/
		printf(ff, "%s", l);
		printf(ff, "<link rel=\"stylesheet\" href=\"yafc.css\" type=\"text/css\">\n")
	 elsif l =~ /<body>/
		printf(ff, "%s", l);
		printf(ff, "<div class=\"main\">\n");
	 elsif l =~ /<\/body>/
		printf(ff, "</div>\n");
		printf(ff, "%s", l);
	 else
		printf(ff, "%s", l);
	 end
  end
	 
  f.close
  ff.close

  File.rename(filename + ".2", filename);
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

