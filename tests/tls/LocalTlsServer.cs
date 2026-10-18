using System;
using System.IO;
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Authentication;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading;

#pragma warning disable SYSLIB0057 // Compatible with .NET Framework and older PowerShell 7 runtimes.

// A short-lived, deliberately untrusted server certificate. No certificate is
// installed in a Windows store. Schannel requires a temporary Windows-managed
// private key; importing without PersistKeySet removes it on certificate disposal.
public sealed class PiwikLocalTlsServer : IDisposable
{
    private readonly RSA key;
    private readonly X509Certificate2 certificate;
    private readonly TcpListener listener;
    private readonly Thread worker;
    private volatile bool stopping;
    public int Port { get; private set; }
    public string LastError { get; private set; }

    public PiwikLocalTlsServer()
    {
        key = RSA.Create();
        key.KeySize = 2048;
        var request = new CertificateRequest("CN=127.0.0.1", key,
            HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
        var names = new SubjectAlternativeNameBuilder();
        names.AddIpAddress(IPAddress.Loopback);
        request.CertificateExtensions.Add(names.Build());
        request.CertificateExtensions.Add(new X509BasicConstraintsExtension(false, false, 0, true));
        request.CertificateExtensions.Add(new X509KeyUsageExtension(
            X509KeyUsageFlags.DigitalSignature | X509KeyUsageFlags.KeyEncipherment, true));
        var usages = new OidCollection();
        usages.Add(new Oid("1.3.6.1.5.5.7.3.1"));
        request.CertificateExtensions.Add(new X509EnhancedKeyUsageExtension(usages, true));
        using (var generated = request.CreateSelfSigned(DateTimeOffset.UtcNow.AddMinutes(-5),
            DateTimeOffset.UtcNow.AddDays(1)))
        {
            var pfx = generated.Export(X509ContentType.Pfx);
            try
            {
                certificate = new X509Certificate2(pfx, (string)null,
                    X509KeyStorageFlags.UserKeySet);
            }
            finally { Array.Clear(pfx, 0, pfx.Length); }
        }
        listener = new TcpListener(IPAddress.Loopback, 0);
        try
        {
            listener.Start();
            Port = ((IPEndPoint)listener.LocalEndpoint).Port;
            worker = new Thread(Serve);
            worker.IsBackground = true;
            worker.Start();
        }
        catch
        {
            listener.Stop();
            certificate.Dispose();
            key.Dispose();
            throw;
        }
    }

    private void Serve()
    {
        while (!stopping)
        {
            try
            {
                using (var client = listener.AcceptTcpClient())
                {
                    client.ReceiveTimeout = 5000;
                    client.SendTimeout = 5000;
                    using (var tls = new SslStream(client.GetStream(), false))
                    {
                        tls.ReadTimeout = 5000;
                        tls.WriteTimeout = 5000;
                        tls.AuthenticateAsServer(certificate, false, SslProtocols.Tls12, false);
                        // Only consume request headers. These tests send GETs.
                        using (var reader = new StreamReader(tls, Encoding.ASCII, false, 1024, true))
                        {
                            string line;
                            bool stall = false;
                            int count = 0;
                            do
                            {
                                line = reader.ReadLine();
                                if (count == 0 && line != null && line.StartsWith("GET /stall ")) stall = true;
                                if (++count > 100) throw new IOException("Too many headers");
                            } while (!String.IsNullOrEmpty(line));
                            if (stall)
                            {
                                // WinHTTP timers are not precise wall-clock deadlines.
                                // Allow timer granularity while keeping fixture shutdown prompt.
                                var deadline = DateTime.UtcNow.AddSeconds(8);
                                while (!stopping && DateTime.UtcNow < deadline) Thread.Sleep(25);
                            }
                        }
                        var reply = Encoding.ASCII.GetBytes(
                            "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n");
                        tls.Write(reply, 0, reply.Length);
                        tls.Flush();
                    }
                }
            }
            // A strict client rejecting this certificate is an expected test path.
            catch (AuthenticationException error) { LastError = error.ToString(); }
            catch (IOException error) { LastError = error.ToString(); }
            catch (SocketException) { if (stopping) return; }
            catch (ObjectDisposedException) { if (stopping) return; }
        }
    }

    public void Dispose()
    {
        stopping = true;
        listener.Stop();
        if (!worker.Join(7000)) throw new TimeoutException("TLS fixture did not stop");
        certificate.Dispose();
        key.Dispose();
    }
}
