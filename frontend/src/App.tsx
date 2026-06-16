import { useOrderBook } from './hooks/useOrderBook';
import { DepthChart } from './components/DepthChart';
import { TradeFeed } from './components/TradeFeed';

const WS_URL = 'ws://localhost:8080';

function App() {
  const { orderBook, trades, isConnected, lastPrice, priceChange } = useOrderBook(WS_URL);

  const formatPrice = (price: number | null | undefined) => {
    if (price === null || price === undefined) return '--';
    return price.toFixed(2);
  };

  const formatQuantity = (qty: number | null | undefined) => {
    if (qty === null || qty === undefined) return '--';
    if (qty >= 1000000) return (qty / 1000000).toFixed(3) + 'M';
    if (qty >= 1000) return (qty / 1000).toFixed(3) + 'K';
    return qty.toFixed(4);
  };

  const bestBid = orderBook?.bids?.[0];
  const bestAsk = orderBook?.asks?.[0];
  const spread = bestBid && bestAsk ? bestAsk.price - bestBid.price : null;

  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        padding: '16px',
        display: 'flex',
        flexDirection: 'column',
        gap: '16px',
        background: '#0a0e17',
      }}
    >
      <header
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '0 8px',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <h1
            style={{
              fontSize: '20px',
              fontWeight: 700,
              color: '#e8ecf3',
              margin: 0,
            }}
          >
            HFT Trading Monitor
          </h1>
          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '4px 12px',
              background: isConnected ? 'rgba(0, 212, 152, 0.15)' : 'rgba(255, 80, 100, 0.15)',
              borderRadius: '20px',
            }}
          >
            <div
              style={{
                width: '8px',
                height: '8px',
                borderRadius: '50%',
                background: isConnected ? '#00d498' : '#ff5064',
                animation: isConnected ? 'pulse 2s infinite' : 'none',
              }}
            />
            <span
              style={{
                fontSize: '12px',
                color: isConnected ? '#00d498' : '#ff5064',
                fontWeight: 500,
              }}
            >
              {isConnected ? '已连接' : '断开连接'}
            </span>
          </div>
        </div>

        {orderBook && (
          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '32px',
            }}
          >
            <div style={{ textAlign: 'center' }}>
              <div
                style={{
                  fontSize: '11px',
                  color: '#6b7a99',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                  marginBottom: '4px',
                }}
              >
                交易对
              </div>
              <div
                style={{
                  fontSize: '18px',
                  fontWeight: 600,
                  color: '#e8ecf3',
                }}
              >
                {orderBook.symbol}
              </div>
            </div>

            <div style={{ textAlign: 'center' }}>
              <div
                style={{
                  fontSize: '11px',
                  color: '#6b7a99',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                  marginBottom: '4px',
                }}
              >
                最新价
              </div>
              <div
                style={{
                  fontSize: '24px',
                  fontWeight: 700,
                  color: priceChange >= 0 ? '#00d498' : '#ff5064',
                  fontFamily: 'Monaco, monospace',
                }}
              >
                {formatPrice(lastPrice)}
              </div>
            </div>

            <div style={{ textAlign: 'center' }}>
              <div
                style={{
                  fontSize: '11px',
                  color: '#6b7a99',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                  marginBottom: '4px',
                }}
              >
                涨跌
              </div>
              <div
                style={{
                  fontSize: '18px',
                  fontWeight: 600,
                  color: priceChange >= 0 ? '#00d498' : '#ff5064',
                  fontFamily: 'Monaco, monospace',
                }}
              >
                {priceChange >= 0 ? '+' : ''}
                {formatPrice(priceChange || 0)}
              </div>
            </div>

            <div style={{ textAlign: 'center' }}>
              <div
                style={{
                  fontSize: '11px',
                  color: '#6b7a99',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                  marginBottom: '4px',
                }}
              >
                价差
              </div>
              <div
                style={{
                  fontSize: '18px',
                  fontWeight: 600,
                  color: '#e8ecf3',
                  fontFamily: 'Monaco, monospace',
                }}
              >
                {formatPrice(spread)}
              </div>
            </div>
          </div>
        )}
      </header>

      {orderBook && (
        <div
          style={{
            display: 'flex',
            gap: '12px',
            padding: '12px 16px',
            background: '#0d1320',
            borderRadius: '8px',
          }}
        >
          <div style={{ flex: 1, textAlign: 'center' }}>
            <div
              style={{
                fontSize: '10px',
                color: '#6b7a99',
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
                marginBottom: '4px',
              }}
            >
              买一价
            </div>
            <div
              style={{
                fontSize: '20px',
                fontWeight: 700,
                color: '#00d498',
                fontFamily: 'Monaco, monospace',
              }}
            >
              {formatPrice(bestBid?.price)}
            </div>
            <div
              style={{
                fontSize: '12px',
                color: '#6b7a99',
                fontFamily: 'Monaco, monospace',
              }}
            >
              {formatQuantity(bestBid?.quantity)}
            </div>
          </div>

          <div
            style={{
              width: '1px',
              background: 'rgba(107, 122, 153, 0.2)',
            }}
          />

          <div style={{ flex: 1, textAlign: 'center' }}>
            <div
              style={{
                fontSize: '10px',
                color: '#6b7a99',
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
                marginBottom: '4px',
              }}
            >
              卖一价
            </div>
            <div
              style={{
                fontSize: '20px',
                fontWeight: 700,
                color: '#ff5064',
                fontFamily: 'Monaco, monospace',
              }}
            >
              {formatPrice(bestAsk?.price)}
            </div>
            <div
              style={{
                fontSize: '12px',
                color: '#6b7a99',
                fontFamily: 'Monaco, monospace',
              }}
            >
              {formatQuantity(bestAsk?.quantity)}
            </div>
          </div>

          <div
            style={{
              width: '1px',
              background: 'rgba(107, 122, 153, 0.2)',
            }}
          />

          <div style={{ flex: 1, textAlign: 'center' }}>
            <div
              style={{
                fontSize: '10px',
                color: '#6b7a99',
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
                marginBottom: '4px',
              }}
            >
              买盘档位
            </div>
            <div
              style={{
                fontSize: '20px',
                fontWeight: 700,
                color: '#e8ecf3',
                fontFamily: 'Monaco, monospace',
              }}
            >
              {orderBook.bids.length}
            </div>
          </div>

          <div
            style={{
              width: '1px',
              background: 'rgba(107, 122, 153, 0.2)',
            }}
          />

          <div style={{ flex: 1, textAlign: 'center' }}>
            <div
              style={{
                fontSize: '10px',
                color: '#6b7a99',
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
                marginBottom: '4px',
              }}
            >
              卖盘档位
            </div>
            <div
              style={{
                fontSize: '20px',
                fontWeight: 700,
                color: '#e8ecf3',
                fontFamily: 'Monaco, monospace',
              }}
            >
              {orderBook.asks.length}
            </div>
          </div>

          <div
            style={{
              width: '1px',
              background: 'rgba(107, 122, 153, 0.2)',
            }}
          />

          <div style={{ flex: 1, textAlign: 'center' }}>
            <div
              style={{
                fontSize: '10px',
                color: '#6b7a99',
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
                marginBottom: '4px',
              }}
            >
              序列号
            </div>
            <div
              style={{
                fontSize: '20px',
                fontWeight: 700,
                color: '#6b7a99',
                fontFamily: 'Monaco, monospace',
              }}
            >
              #{orderBook.sequence}
            </div>
          </div>
        </div>
      )}

      <div
        style={{
          flex: 1,
          display: 'flex',
          gap: '16px',
          minHeight: 0,
        }}
      >
        <div
          style={{
            flex: '0 0 auto',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
          }}
        >
          {orderBook ? (
            <DepthChart
              bids={orderBook.bids}
              asks={orderBook.asks}
              lastPrice={lastPrice}
              priceChange={priceChange}
              width={700}
              height={Math.max(500, (orderBook.bids.length + orderBook.asks.length) * 28 + 120)}
            />
          ) : (
            <div
              style={{
                width: '700px',
                height: '500px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                background: '#0d1320',
                borderRadius: '8px',
                color: '#6b7a99',
                fontSize: '14px',
              }}
            >
              正在连接订单簿...
            </div>
          )}
        </div>

        <div
          style={{
            flex: 1,
            minWidth: '320px',
            display: 'flex',
            flexDirection: 'column',
          }}
        >
          <TradeFeed trades={trades} maxItems={200} />
        </div>
      </div>

      <style>{`
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
      `}</style>
    </div>
  );
}

export default App;
