#include "datetimeus.h"

const char *datetimeus::c_weekdays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

/* constructors */

datetimeus::datetimeus(unsigned int a_year, unsigned int a_month, unsigned int a_day, unsigned int a_hour, unsigned int a_minute, unsigned int a_second)
{
	memset(&m_timeval, 0, sizeof(struct timeval)); // clear our timeval

	struct tm l_tm;
	memset(&l_tm, 0, sizeof(struct tm)); // create and zero out a new tm

	l_tm.tm_year = a_year - 1900;
	l_tm.tm_mon = a_month - 1;
	l_tm.tm_mday = a_day;
	l_tm.tm_hour = a_hour;
	l_tm.tm_min = a_minute;
	l_tm.tm_sec = a_second;
	
	m_timeval.tv_sec = mktime(&l_tm);
}

datetimeus::datetimeus(double a_double_time)
{
	set_double_time(a_double_time);
}

datetimeus::datetimeus(const datetimeus& a_datetimeus)
{
	copy_remote(a_datetimeus);
}

void datetimeus::copy_remote(const datetimeus& a_datetimeus)
{
	// copy data from remote instance (used by operator= and copy ctor)
	memcpy(&m_timeval, &a_datetimeus.m_timeval, sizeof(struct timeval));
}

/* operators */

datetimeus& datetimeus::operator=(const datetimeus& a_datetimeus)
{
	// check for self-assignment
	if (this == &a_datetimeus) {
		std::cerr << "Self-assignment detected." << std::endl;
		exit(-1);
	}

	copy_remote(a_datetimeus);

	return *this;
}

datetimeus::operator double()
{
	double ret = (double)m_timeval.tv_sec + ((double)m_timeval.tv_usec / 1000000.0);
	return ret;
}

std::ostream& operator<<(std::ostream &os, const datetimeus& a_datetimeus)
{
	os << datetimeus::c_weekdays[a_datetimeus.wday()] << " ";
	os << std::setfill('0') << std::setw(4) << a_datetimeus.year() << "-";
	os << std::setfill('0') << std::setw(2) << a_datetimeus.month() << "-";
	os << std::setfill('0') << std::setw(2) << a_datetimeus.day() << " ";
	os << std::setfill('0') << std::setw(2) << a_datetimeus.hour() << ":";
	os << std::setfill('0') << std::setw(2) << a_datetimeus.minute() << ":";
	os << std::setfill('0') << std::setw(2) << a_datetimeus.second() << " ";
	os << std::setfill('0') << std::setw(6) << a_datetimeus.usec() << " ";
	os << a_datetimeus.tzstr() << " GMT";
	os.setf(std::ios::fixed); // make sure we print "7.50" and not "7.50000e+00" or some shit
	os.precision(2);
	if (a_datetimeus.gmtoff() < 0)
		os << a_datetimeus.gmtoff();
	else
		os << "+" << a_datetimeus.gmtoff();

	return os;
}

/* static methods */

datetimeus datetimeus::now()
{
	struct timeval tv;
	datetimeus ret;

	gettimeofday(&tv, NULL);
	memcpy(&ret.m_timeval, &tv, sizeof(struct timeval));

	return ret;
}

std::string datetimeus::now_as_std_string(int a_showflags)
{
	datetimeus l_now = datetimeus::now();
	return l_now.as_std_string(a_showflags);
}

std::string datetimeus::now_as_std_string_iso8601()
{
	datetimeus l_now = datetimeus::now();
	return l_now.as_std_string_iso8601();
}

/* component calls */

int datetimeus::wday() const
{
	struct tm *l_tm = localtime(&m_timeval.tv_sec);
	return l_tm->tm_wday;
}

int datetimeus::yday() const
{
	struct tm *l_tm = localtime(&m_timeval.tv_sec);
	return l_tm->tm_yday;
}

int datetimeus::year() const
{
	struct tm *l_tm = localtime(&m_timeval.tv_sec);
	return l_tm->tm_year + 1900;
}

int datetimeus::month() const
{
	struct tm *l_tm = localtime(&m_timeval.tv_sec);
	return l_tm->tm_mon + 1;
}

int datetimeus::day() const
{
	struct tm *l_tm = localtime(&m_timeval.tv_sec);
	return l_tm->tm_mday;
}

int datetimeus::hour() const
{
	struct tm *l_tm = localtime(&m_timeval.tv_sec);
	return l_tm->tm_hour;
}

int datetimeus::minute() const
{
	struct tm *l_tm = localtime(&m_timeval.tv_sec);
	return l_tm->tm_min;
}

int datetimeus::second() const
{
	struct tm *l_tm = localtime(&m_timeval.tv_sec);
	return l_tm->tm_sec;
}

int datetimeus::usec() const
{
	return m_timeval.tv_usec;
}

double datetimeus::gmtoff() const
{
	struct tm *l_tm = localtime(&m_timeval.tv_sec);
	return (double)l_tm->tm_gmtoff / 3600.0;
}

int datetimeus::isdst() const
{
	struct tm *l_tm = localtime(&m_timeval.tv_sec);
	return l_tm->tm_isdst;
}

std::string datetimeus::tzstr() const
{
	struct tm *l_tm = localtime(&m_timeval.tv_sec);
	return std::string(l_tm->tm_zone);
}

std::string datetimeus::as_std_string(int a_showflags) const
{
	std::stringstream l_ss_out;

	if ((a_showflags & SHOW_WEEKDAY) || (a_showflags & SHOW_ALL)) {
		l_ss_out << c_weekdays[wday()] << " ";
	}
	if ((a_showflags & SHOW_DATE) || (a_showflags & SHOW_ALL)) {
		l_ss_out << std::setfill('0') << std::setw(4) << year() << "-";
		l_ss_out << std::setfill('0') << std::setw(2) << month() << "-";
		l_ss_out << std::setfill('0') << std::setw(2) << day() << " ";
	}
	if ((a_showflags & SHOW_TIME) || (a_showflags & SHOW_ALL)) {
		l_ss_out << std::setfill('0') << std::setw(2) << hour() << ":";
		l_ss_out << std::setfill('0') << std::setw(2) << minute() << ":";
		l_ss_out << std::setfill('0') << std::setw(2) << second() << " ";
	}
	if ((a_showflags & SHOW_USECS) || (a_showflags & SHOW_ALL)) {
		l_ss_out << std::setfill('0') << std::setw(6) << usec() << " ";
	}
	if ((a_showflags & SHOW_TZDATA) || (a_showflags & SHOW_ALL)) {
		l_ss_out << tzstr() << " GMT";
		l_ss_out.setf(std::ios::fixed);
		l_ss_out.precision(2);
		if (gmtoff() < 0)
			l_ss_out << gmtoff();
		else
			l_ss_out << "+" << gmtoff();
	}

	std::string l_out = l_ss_out.str();

	// trim trailing space if it exists
	if (l_out.size() > 0) {
		std::string::iterator l_it = l_out.end() - 1;
		if (*l_it == ' ')
			l_out.erase(l_it);
	}

	return l_out;
}

std::string datetimeus::as_std_string_iso8601() const
{
	std::stringstream l_ss_out;

	l_ss_out << std::setfill('0') << std::setw(4) << year() << "-";
	l_ss_out << std::setfill('0') << std::setw(2) << month() << "-";
	l_ss_out << std::setfill('0') << std::setw(2) << day() << "T";
	l_ss_out << std::setfill('0') << std::setw(2) << hour() << ":";
	l_ss_out << std::setfill('0') << std::setw(2) << minute() << ":";
	l_ss_out << std::setfill('0') << std::setw(2) << second() << ",";
	l_ss_out << std::setfill('0') << std::setw(6) << usec();

	return l_ss_out.str();
}

void datetimeus::delta_time(double a_delta)
{
	double l_double_time = double(*this);
	l_double_time += a_delta;
	set_double_time(l_double_time);
}

void datetimeus::time_for_now()
{
	gettimeofday(&m_timeval, NULL);
}

void datetimeus::set_double_time(double a_double)
{
	double l_int;
	double l_frac = modf(a_double, &l_int);

	int l_usec = (int)(l_frac * 1000000.0);
	int l_sec = (int)l_int;

	m_timeval.tv_usec = l_usec;
	m_timeval.tv_sec = l_sec;
}

