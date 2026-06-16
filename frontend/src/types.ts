export interface PriceLevel {
  price: number;
  quantity: number;
  orders: number;
}

export interface Trade {
  tradeId: number;
  symbol: string;
  price: number;
  quantity: number;
  timestamp: number;
  direction?: 'buy' | 'sell';
}

export interface OrderBookSnapshot {
  type: 'snapshot';
  symbol: string;
  timestamp: number;
  sequence: number;
  bids: PriceLevel[];
  asks: PriceLevel[];
}

export interface TradeMessage {
  type: 'trade';
  symbol: string;
  tradeId: number;
  price: number;
  quantity: number;
  timestamp: number;
}

export type WSMessage = OrderBookSnapshot | TradeMessage;

export interface OrderBookState {
  symbol: string;
  bids: PriceLevel[];
  asks: PriceLevel[];
  lastUpdate: number;
  sequence: number;
}
