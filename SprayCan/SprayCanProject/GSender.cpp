#include "Gsender.h"
Gsender* Gsender::_instance = 0;
Gsender::Gsender() {}
Gsender* Gsender::Instance()
{
  if (_instance == 0)
    _instance = new Gsender;
  return _instance;
}

Gsender* Gsender::Subject(const char* subject)
{
  delete [] _subject;
  _subject = new char[strlen(subject) + 1];
  strcpy(_subject, subject);
  return _instance;
}
Gsender* Gsender::Subject(const String &subject)
{
  return Subject(subject.c_str());
}

bool Gsender::AwaitSMTPResponse(WiFiClientSecure &client, const String &resp, uint16_t timeOut)
{
  uint32_t ts = millis();
  while (!client.available())
  {
    if (millis() > (ts + timeOut)) {
      _error = "SMTP Response TIMEOUT!";
      return false;
    }
  }
  _serverResponce = client.readStringUntil('\n');
  if (resp && _serverResponce.indexOf(resp) == -1) return false;
  return true;
}

String Gsender::getLastResponce()
{
  return _serverResponce;
}

const char* Gsender::getError()
{
  return _error;
}

bool Gsender::Send(const String &email_login, const String &email_password, const String &from, const String &to, const String &message)
{
  WiFiClientSecure client;
  if (!client.connect(SMTP_SERVER, SMTP_PORT)) {
    _error = "Could not connect to mail server";
    return false;
  }
  if (!AwaitSMTPResponse(client, "220")) {
    _error = "Connection Error";
    return false;
  }

  client.println("HELO friend");
  if (!AwaitSMTPResponse(client, "250")) {
    _error = "identification error";
    return false;
  }

  client.println("AUTH LOGIN");
  AwaitSMTPResponse(client);

  client.println(email_login);
  AwaitSMTPResponse(client);

  client.println(email_password);
  if (!AwaitSMTPResponse(client, "235")) {
    _error = "SMTP AUTH error";
    return false;
  }

  String mailFrom = "MAIL FROM: <" + String(from) + '>';
  client.println(mailFrom);
  AwaitSMTPResponse(client);

  String rcpt = "RCPT TO: <" + to + '>';
  client.println(rcpt);
  AwaitSMTPResponse(client);

  client.println("DATA");
  if (!AwaitSMTPResponse(client, "354")) {
    _error = "SMTP DATA error";
    return false;
  }

  client.println("From: <" + String(from) + '>');
  client.println("To: <" + to + '>');

  client.print("Subject: ");
  client.println(_subject);

  client.println("Mime-Version: 1.0");
  client.println("Content-Type: text/html; charset=\"UTF-8\"");
  client.println("Content-Transfer-Encoding: 7bit");
  client.println();
  String body = "<!DOCTYPE html><html lang=\"en\">" + message + "</html>";
  client.println(body);
  client.println(".");
  if (!AwaitSMTPResponse(client, "250")) {
    _error = "Sending message error";
    return false;
  }
  client.println("QUIT");
  if (!AwaitSMTPResponse(client, "221")) {
    _error = "SMTP QUIT error";
    return false;
  }
  return true;
}
