// These implementations are subject to the Boost License
// (http://www.boost.org/users/license.html)

#include <algorithm>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/system/error_code.hpp>

// Backport of boost filesystem API's not available on lab computers (1.60
// -> 1.58)
namespace boost_backport {

using namespace boost::filesystem;
using boost::system::error_code;

const char separator = '/';
const char dot = '.';

// stub impl, just throws exception
static bool error(int error_num, const path &p, error_code *ec,
                  const char *msg) {
  if (error_num != 0)
    throw std::runtime_error(msg);
  return error_num != 0;
}

// boost::filesystem::path::lexically_normal
// retrieved from
// https://github.com/boostorg/filesystem/blob/23b79b9459c862b819869b78b24121b184d5c74d/src/path.cpp#L416
static path lexically_normal(const path &ths) {
  if (ths.empty())
    return ths;

  path temp;
  path::iterator start(ths.begin());
  path::iterator last(ths.end());
  path::iterator stop(last--);
  for (path::iterator itr(start); itr != stop; ++itr) {
    // ignore "." except at start and last
    if (itr->native().size() == 1 && (itr->native())[0] == dot &&
        itr != start && itr != last)
      continue;

    // ignore a name and following ".."
    if (!temp.empty() && itr->native().size() == 2 &&
        (itr->native())[0] == dot && (itr->native())[1] == dot) // dot dot
    {
      boost::filesystem::path::string_type lf(temp.filename().native());
      if (lf.size() > 0 &&
          (lf.size() != 1 || (lf[0] != dot && lf[0] != separator)) &&
          (lf.size() != 2 || (lf[0] != dot && lf[1] != dot))) {
        temp.remove_filename();
        //// if not root directory, must also remove "/" if any
        // if (temp.native().size() > 0
        //  && temp.native()[temp.native().size()-1]
        //    == separator)
        //{
        //  string_type::size_type rds(
        //    root_directory_start(temp.native(), temp.native().size()));
        //  if (rds == string_type::npos
        //    || rds != temp.native().size()-1)
        //  {
        //    temp.m_pathname.erase(temp.native().size()-1);
        //  }
        //}

        path::iterator next(itr);
        if (temp.empty() && ++next != stop && next == last &&
            *last == detail::dot_path()) {
          temp /= detail::dot_path();
        }
        continue;
      }
    }

    temp /= *itr;
  };

  if (temp.empty())
    temp /= detail::dot_path();
  return temp;
}

// boost::filesystem::detail::weakly_canonical
// retrieved from
// https://github.com/boostorg/filesystem/blob/23b79b9459c862b819869b78b24121b184d5c74d/src/operations.cpp#L2003
static path weakly_canonical(const path &p, error_code *ec) {
  path head(p);
  path tail;
  error_code tmp_ec;
  path::iterator itr = p.end();

  for (; !head.empty(); --itr) {
    file_status head_status = status(head, tmp_ec);
    if (error(head_status.type() == status_error, head, ec,
              "boost::filesystem::weakly_canonical"))
      return path();
    if (head_status.type() != file_not_found)
      break;
    head.remove_filename();
  }

  bool tail_has_dots = false;
  for (; itr != p.end(); ++itr) {
    tail /= *itr;
    // for a later optimization, track if any dot or dot-dot elements are
    // present
    if (itr->native().size() <= 2 && itr->native()[0] == dot &&
        (itr->native().size() == 1 || itr->native()[1] == dot))
      tail_has_dots = true;
  }

  if (head.empty())
    return lexically_normal(p);
  head = canonical(head, tmp_ec);
  if (error(tmp_ec.value(), head, ec, "boost::filesystem::weakly_canonical"))
    return path();
  return tail.empty() ? head
                      : (tail_has_dots // optimization: only normalize if tail
                                       // had dot or dot-dot element
                             ? lexically_normal(head / tail)
                             : head / tail);
}

// boost::filesystem::path::lexically_relative
// retrieved from
// https://github.com/boostorg/filesystem/blob/a682eaa476cf0b4e992884d32dd2ddcfb0b6b1aa/src/path.cpp#L398
// adapted to static function, calls std::mismatch instead of boost's own
// version
static path lexically_relative(const path &ths, const path &base) {
  std::pair<path::iterator, path::iterator> mm =
      std::mismatch(ths.begin(), ths.end(), base.begin(), base.end());
  if (mm.first == ths.begin() && mm.second == base.begin())
    return path();
  if (mm.first == ths.end() && mm.second == base.end())
    return detail::dot_path();
  path tmp;
  for (; mm.second != base.end(); ++mm.second)
    tmp /= detail::dot_dot_path();
  for (; mm.first != ths.end(); ++mm.first)
    tmp /= *mm.first;
  return tmp;
}

// boost::filesystem::detail::relative
// retrieved from
// https://github.com/boostorg/filesystem/blob/23b79b9459c862b819869b78b24121b184d5c74d/src/operations.cpp#L1653
path relative(const path &p, const path &base) {
  error_code *ec = 0;
  error_code tmp_ec;
  path wc_base(weakly_canonical(base, &tmp_ec));
  if (error(tmp_ec.value(), base, ec, "boost::filesystem::relative"))
    return path();
  path wc_p(weakly_canonical(p, &tmp_ec));
  if (error(tmp_ec.value(), base, ec, "boost::filesystem::relative"))
    return path();
  return lexically_relative(wc_p, wc_base);
}

} // namespace boost_backport
