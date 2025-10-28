#include <httplib.h>
#include <iostream>

using namespace std;
int main()
{
    // HTTP
    httplib::Client cli("http://127.0.0.1:8000");
    
    auto res = cli.Get("/");

    cout<<res->status<<endl;
    cout<<res->body<<endl;
}