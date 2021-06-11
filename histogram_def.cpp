#include "histogram_def.h"

bool histogram_def::match(std::u32string u32key, std::string *displayString) const
{
    if ( flags.lowercase ){
        u32key = utf32_lowercase( u32key );
    }

    if ( flags.numeric ) {
        u32key = utf32_extract_numeric( u32key );
    }

    /* TODO: When we have the ability to do regular expressions in utf32, do that here.
     * We don't have that, so do the rest in utf8 */

    /* Convert match string to u8key */
    std::string u8key = convert_utf32_to_utf8( u32key );

    /* If a string is required and it is not present, return */
    if (require.size() > 0 && u8key.find_first_of(require)==std::string::npos){
        std::cerr << "fail1\n";
        return false;
    }

    /* Check for pattern */
    if (pattern.size() > 0){
        std::smatch m {};
        std::regex_search( u8key, m, this->reg);
        if (m.empty()==true){       // match does not exist
            return false;           // regex not found
        }
        u8key = m.str();
    }

    if (displayString) {
        *displayString = u8key;
    }
    return true;
}


bool histogram_def::match(std::string u32key, std::string *displayString) const
{
    return match( convert_utf8_to_utf32( u32key), displayString);
}


std::ostream & operator << (std::ostream &os, const histogram_def &hd)
{
    os << "<histogram_def( name:" << hd.name << " feature:" << hd.feature << " pattern:" << hd.pattern
       << " require:" << hd.require << " suffix:" << hd.suffix << ")>";
    return os;
}
