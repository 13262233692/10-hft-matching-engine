import { useState, useEffect, useCallback, useRef } from 'react';
import type { WSMessage, OrderBookState, Trade, TradeMessage, OrderBookSnapshot } from '../types';

interface UseOrderBookReturn {
  orderBook: OrderBookState | null;
  trades: Trade[];
  isConnected: boolean;
  lastPrice: number | null;
  priceChange: number;
}

export function useOrderBook(wsUrl: string, maxTrades: number = 100): UseOrderBookReturn {
  const [orderBook, setOrderBook] = useState<OrderBookState | null>(null);
  const [trades, setTrades] = useState<Trade[]>([]);
  const [isConnected, setIsConnected] = useState(false);
  const [lastPrice, setLastPrice] = useState<number | null>(null);
  const [priceChange, setPriceChange] = useState(0);
  const wsRef = useRef<WebSocket | null>(null);
  const prevPriceRef = useRef<number | null>(null);

  const handleMessage = useCallback((event: MessageEvent) => {
    try {
      const data: WSMessage = JSON.parse(event.data);

      if (data.type === 'snapshot') {
        const snapshot = data as OrderBookSnapshot;
        setOrderBook({
          symbol: snapshot.symbol,
          bids: snapshot.bids,
          asks: snapshot.asks,
          lastUpdate: snapshot.timestamp,
          sequence: snapshot.sequence,
        });
      } else if (data.type === 'trade') {
        const tradeMsg = data as TradeMessage;
        const trade: Trade = {
          ...tradeMsg,
          direction: prevPriceRef.current
            ? tradeMsg.price > prevPriceRef.current
              ? 'buy'
              : tradeMsg.price < prevPriceRef.current
              ? 'sell'
              : prevPriceRef.current
              ? 'buy'
              : undefined
            : undefined,
        };

        if (prevPriceRef.current !== null) {
          setPriceChange(tradeMsg.price - prevPriceRef.current);
        }
        prevPriceRef.current = tradeMsg.price;
        setLastPrice(tradeMsg.price);

        setTrades((prev) => {
          const updated = [trade, ...prev];
          return updated.slice(0, maxTrades);
        });
      }
    } catch (error) {
      console.error('Failed to parse WebSocket message:', error);
    }
  }, [maxTrades]);

  useEffect(() => {
    let retryCount = 0;
    let reconnectTimeout: NodeJS.Timeout | null = null;

    const connect = () => {
      try {
        const ws = new WebSocket(wsUrl);
        wsRef.current = ws;

        ws.onopen = () => {
          setIsConnected(true);
          retryCount = 0;
          console.log('[WS] Connected to order book feed');
        };

        ws.onmessage = handleMessage;

        ws.onerror = (error) => {
          console.error('[WS] WebSocket error:', error);
        };

        ws.onclose = () => {
          setIsConnected(false);
          console.log('[WS] Disconnected. Reconnecting...');

          const delay = Math.min(1000 * Math.pow(2, retryCount), 10000);
          retryCount++;

          reconnectTimeout = setTimeout(connect, delay);
        };
      } catch (error) {
        console.error('[WS] Failed to create WebSocket:', error);
      }
    };

    connect();

    return () => {
      if (reconnectTimeout) {
        clearTimeout(reconnectTimeout);
      }
      if (wsRef.current) {
        wsRef.current.close();
      }
    };
  }, [wsUrl, handleMessage]);

  return { orderBook, trades, isConnected, lastPrice, priceChange };
}
