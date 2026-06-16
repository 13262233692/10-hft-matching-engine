#!/usr/bin/env python3
import socket
import time
import random
import struct

SOH = b'\x01'

def build_fix_new_order(client_order_id, side, price, quantity, symbol):
    side_str = '1' if side == 'BUY' else '2'
    price_str = f"{price:.4f}"
    qty_str = str(quantity)
    
    body_fields = [
        ('35', 'D'),
        ('49', 'TEST_CLIENT'),
        ('56', 'HFT_ENGINE'),
        ('34', str(client_order_id)),
        ('52', time.strftime('%Y%m%d-%H:%M:%S.000')),
        ('11', str(client_order_id)),
        ('54', side_str),
        ('55', symbol),
        ('38', qty_str),
        ('44', price_str),
        ('40', '2'),
        ('59', '0'),
    ]
    
    body = ''
    for tag, val in body_fields:
        body += f'{tag}={val}' + chr(1)
    
    body_len = len(body)
    
    header = f'8=FIX.4.4{chr(1)}9={body_len}{chr(1)}'
    msg = header + body
    
    checksum = sum(ord(c) for c in msg) % 256
    msg += f'10={checksum:03d}{chr(1)}'
    
    return msg.encode('ascii')

def send_orders():
    host = 'localhost'
    port = 12345
    
    print(f'Connecting to {host}:{port}...')
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    print('Connected!')
    
    try:
        base_price = 100.0
        order_id = 1000
        
        print('Seeding initial order book...')
        
        for i in range(10):
            price = base_price - 0.01 * (i + 1)
            qty = random.randint(5, 50)
            msg = build_fix_new_order(order_id, 'BUY', price, qty, 'BTC-USDT')
            sock.sendall(msg)
            order_id += 1
            time.sleep(0.01)
        
        for i in range(10):
            price = base_price + 0.01 * (i + 1)
            qty = random.randint(5, 50)
            msg = build_fix_new_order(order_id, 'SELL', price, qty, 'BTC-USDT')
            sock.sendall(msg)
            order_id += 1
            time.sleep(0.01)
        
        print('Initial order book seeded. Starting random order flow...')
        print('Press Ctrl+C to stop')
        
        while True:
            side = random.choice(['BUY', 'SELL'])
            if side == 'BUY':
                price = base_price + random.uniform(-0.05, 0.02)
            else:
                price = base_price + random.uniform(-0.02, 0.05)
            
            price = round(price, 4)
            qty = random.randint(1, 30)
            
            msg = build_fix_new_order(order_id, side, price, qty, 'BTC-USDT')
            sock.sendall(msg)
            
            if random.random() < 0.3:
                trade_qty = random.randint(1, min(qty, 20))
                trade_side = 'SELL' if side == 'BUY' else 'BUY'
                trade_price = price + random.uniform(-0.005, 0.005) if side == 'SELL' else price - random.uniform(-0.005, 0.005)
                trade_price = round(trade_price, 4)
                trade_msg = build_fix_new_order(order_id + 1, trade_side, trade_price, trade_qty, 'BTC-USDT')
                sock.sendall(trade_msg)
                print(f'[TRADE] {trade_side} {trade_qty} @ {trade_price:.4f}')
                order_id += 1
                base_price = trade_price
            
            order_id += 1
            time.sleep(random.uniform(0.05, 0.2))
            
    except KeyboardInterrupt:
        print('\nStopping...')
    finally:
        sock.close()
        print('Disconnected')

if __name__ == '__main__':
    send_orders()
