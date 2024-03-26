import http.server
import os
import ssl


def start_https_server(server_ip: str, server_port: int, server_file: str = None, key_file: str = None) -> None:
    print('Starting HTTPS server at "https://{}:{}"'.format(server_ip, server_port))
    
    os.chdir("data")

    httpd = http.server.HTTPServer((server_ip, server_port), http.server.SimpleHTTPRequestHandler)
    
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile=server_file, keyfile=key_file)

    httpd.socket = context.wrap_socket(httpd.socket, server_side=True)

    httpd.serve_forever()


if __name__ == '__main__':
    start_https_server(
        "0.0.0.0",
        8443,
        server_file="cert.pem",
        key_file="key.pem"
    )