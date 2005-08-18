class LCDRange < Qt::Widget

    signals 'valueChanged(int)'

    slots 'value=(int)', 'setRange(int, int)', 'text=(const char*)'
    
    def initialize(s, parent = nil)
        super(parent)
        init()
        self.text = s
    end
    
    def init
        lcd = Qt::LCDNumber.new(2)
        @slider = Qt::Slider.new(Qt::Horizontal)
        @slider.setRange(0, 99)
        @slider.value = 0
        
        @label = Qt::Label.new
        @label.alignment = Qt::AlignHCenter.to_i | Qt::AlignTop.to_i
        @label.setSizePolicy(Qt::SizePolicy::Preferred, Qt::SizePolicy::Fixed)
            
        connect(@slider, SIGNAL('valueChanged(int)'), lcd, SLOT('display(int)'))
        connect(@slider, SIGNAL('valueChanged(int)'), SIGNAL('valueChanged(int)'))
        
        layout = Qt::VBoxLayout.new
        layout.addWidget(lcd)
        layout.addWidget(@slider)
        setLayout(layout)
        
        setFocusProxy(@slider)
    end
    
    def value
        @slider.value
    end

    def value=( value )
        @slider.value = value
    end
    
    def setRange( minVal, maxVal )
		if minVal < 0 || maxVal > 99 || minVal > maxVal
      		qWarning( "LCDRange::setRange(#{minVal},#{maxVal})\n" +
               		"\tRange must be 0..99\n" +
               		"\tand minVal must not be greater than maxVal" )
			return
		end
        @slider.setRange( minVal, maxVal )
    end
    
    def text=( s )
        @label.text = s
    end

end
