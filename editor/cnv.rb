File.open("z") { |f|

    f.each_line { |l|
	l.strip!
	f = l.split('/')[1][4..-1]

	underscored=false
	cnvr = ""
        f.each_char {|c|
	  if(c=="_")
	    underscored=true
	    next
	  end
	  if(underscored)
	    c = c.upcase
	    underscored=false
          end
	  cnvr += c
	}
	puts '<file alias="'+cnvr+'">'+l+'</file>'
    }
}