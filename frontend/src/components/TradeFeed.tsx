import { useRef, useEffect } from 'react';
import type { Trade } from '../types';

interface TradeFeedProps {
  trades: Trade[];
  maxItems?: number;
}

const formatTime = (timestamp: number) => {
  const date = new Date(timestamp / 1000);
  return date.toLocaleTimeString('zh-CN', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    fractionalSecondDigits: 3,
  } as Intl.DateTimeFormatOptions);
};

const formatPrice = (price: number) => {
  return price.toFixed(2);
};

const formatQuantity = (qty: number) => {
  if (qty >= 1000) return (qty / 1000).toFixed(3) + 'K';
  return qty.toFixed(4);
};

export function TradeFeed({ trades, maxItems = 100 }: TradeFeedProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const prevFirstTradeId = useRef<number | null>(null);

  useEffect(() => {
    if (trades.length > 0 && trades[0].tradeId !== prevFirstTradeId.current) {
      prevFirstTradeId.current = trades[0].tradeId;

      const firstRow = containerRef.current?.firstElementChild as HTMLElement;
      if (firstRow) {
        firstRow.classList.add('flash');
        setTimeout(() => firstRow.classList.remove('flash'), 400);
      }
    }
  }, [trades]);

  const displayTrades = trades.slice(0, maxItems);

  return (
    <div
      style={{
        background: '#0d1320',
        borderRadius: '8px',
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}
    >
      <div
        style={{
          padding: '12px 16px',
          borderBottom: '1px solid rgba(107, 122, 153, 0.2)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}
      >
        <h3
          style={{
            fontSize: '14px',
            fontWeight: 600,
            color: '#e8ecf3',
            margin: 0,
          }}
        >
          最新成交
        </h3>
        <span
          style={{
            fontSize: '11px',
            color: '#6b7a99',
            fontFamily: 'Monaco, monospace',
          }}
        >
          {displayTrades.length} 笔
        </span>
      </div>

      <div
        style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1fr 1fr',
          padding: '8px 16px',
          fontSize: '10px',
          color: '#6b7a99',
          fontFamily: 'Monaco, monospace',
          borderBottom: '1px solid rgba(107, 122, 153, 0.1)',
        }}
      >
        <span>时间</span>
        <span style={{ textAlign: 'right' }}>价格</span>
        <span style={{ textAlign: 'right' }}>数量</span>
      </div>

      <div
        ref={containerRef}
        style={{
          flex: 1,
          overflowY: 'auto',
          overflowX: 'hidden',
        }}
      >
        <style>{`
          .trade-row {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            padding: 6px 16px;
            font-family: Monaco, monospace;
            font-size: 12px;
            transition: background-color 0.3s ease;
          }
          .trade-row:hover {
            background: rgba(107, 122, 153, 0.1);
          }
          .trade-row.flash {
            animation: flashAnimation 0.4s ease-out;
          }
          @keyframes flashAnimation {
            0% { background: rgba(0, 212, 152, 0.3); }
            100% { background: transparent; }
          }
          .trade-row.flash.sell {
            animation: flashAnimationSell 0.4s ease-out;
          }
          @keyframes flashAnimationSell {
            0% { background: rgba(255, 80, 100, 0.3); }
            100% { background: transparent; }
          }
          .trade-time {
            color: #6b7a99;
            font-size: 11px;
          }
          .trade-price {
            text-align: right;
            font-weight: 500;
          }
          .trade-price.buy {
            color: #00d498;
          }
          .trade-price.sell {
            color: #ff5064;
          }
          .trade-qty {
            text-align: right;
            color: #e8ecf3;
          }
          .scrollbar::-webkit-scrollbar {
            width: 6px;
          }
          .scrollbar::-webkit-scrollbar-track {
            background: transparent;
          }
          .scrollbar::-webkit-scrollbar-thumb {
            background: rgba(107, 122, 153, 0.3);
            border-radius: 3px;
          }
          .scrollbar::-webkit-scrollbar-thumb:hover {
            background: rgba(107, 122, 153, 0.5);
          }
        `}</style>

        {displayTrades.length === 0 ? (
          <div
            style={{
              padding: '40px 20px',
              textAlign: 'center',
              color: '#6b7a99',
              fontSize: '12px',
            }}
          >
            等待成交数据...
          </div>
        ) : (
          displayTrades.map((trade) => {
            const direction = trade.direction || 'buy';
            return (
              <div
                key={trade.tradeId}
                className={`trade-row ${direction}`}
              >
                <span className="trade-time">
                  {formatTime(trade.timestamp)}
                </span>
                <span className={`trade-price ${direction}`}>
                  {formatPrice(trade.price)}
                </span>
                <span className="trade-qty">
                  {formatQuantity(trade.quantity)}
                </span>
              </div>
            );
          })
        )}
      </div>
    </div>
  );
}
