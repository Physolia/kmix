#ifndef VerticalText_h
#define VerticalText_h

#include <qwidget.h>

class VerticalText : public QWidget
{
public:
	VerticalText(QWidget * parent, const char * name, WFlags f = 0);
	~VerticalText();

    QSize sizeHint();
	
protected:
	void paintEvent ( QPaintEvent * event );

};


#endif
