import { useEffect, useRef, useMemo } from 'react';
import type { PriceLevel } from '../types';

interface DepthChartProps {
  bids: PriceLevel[];
  asks: PriceLevel[];
  lastPrice?: number | null;
  priceChange?: number;
  width?: number;
  height?: number;
}

interface RenderData {
  maxBidQty: number;
  maxAskQty: number;
  maxTotalQty: number;
  bidLevels: (PriceLevel & { cumulative: number; percent: number })[];
  askLevels: (PriceLevel & { cumulative: number; percent: number })[];
  priceRange: { min: number; max: number };
}

const COLORS = {
  bid: 'rgba(0, 212, 152, 0.85)',
  bidBg: 'rgba(0, 212, 152, 0.15)',
  ask: 'rgba(255, 80, 100, 0.85)',
  askBg: 'rgba(255, 80, 100, 0.15)',
  text: '#e8ecf3',
  textSecondary: '#6b7a99',
  grid: 'rgba(107, 122, 153, 0.15)',
  lastPriceUp: '#00d498',
  lastPriceDown: '#ff5064',
};

export function DepthChart({ bids, asks, lastPrice, priceChange = 0, width = 800, height = 500 }: DepthChartProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const animationRef = useRef<number>();
  const prevDataRef = useRef<RenderData | null>(null);
  const targetDataRef = useRef<RenderData | null>(null);
  const animationProgressRef = useRef(0);

  const padding = { top: 40, right: 120, bottom: 40, left: 120 };
  const chartWidth = width - padding.left - padding.right;
  const chartHeight = height - padding.top - padding.bottom;

  const targetData = useMemo((): RenderData => {
    const bidLevels: (PriceLevel & { cumulative: number; percent: number })[] = [];
    const askLevels: (PriceLevel & { cumulative: number; percent: number })[] = [];

    let maxBidQty = 0;
    let maxAskQty = 0;
    let bidCumulative = 0;
    let askCumulative = 0;

    for (const level of bids) {
      bidCumulative += level.quantity;
      bidLevels.push({ ...level, cumulative: bidCumulative, percent: 0 });
      maxBidQty = Math.max(maxBidQty, bidCumulative);
    }

    for (const level of asks) {
      askCumulative += level.quantity;
      askLevels.push({ ...level, cumulative: askCumulative, percent: 0 });
      maxAskQty = Math.max(maxAskQty, askCumulative);
    }

    const maxTotalQty = Math.max(maxBidQty, maxAskQty, 1);

    for (const level of bidLevels) {
      level.percent = level.cumulative / maxTotalQty;
    }
    for (const level of askLevels) {
      level.percent = level.cumulative / maxTotalQty;
    }

    const allPrices = [...bids.map(b => b.price), ...asks.map(a => a.price)];
    const minPrice = allPrices.length > 0 ? Math.min(...allPrices) : 0;
    const maxPrice = allPrices.length > 0 ? Math.max(...allPrices) : 0;

    return {
      maxBidQty,
      maxAskQty,
      maxTotalQty,
      bidLevels,
      askLevels,
      priceRange: { min: minPrice, max: maxPrice },
    };
  }, [bids, asks]);

  useEffect(() => {
    targetDataRef.current = targetData;
    animationProgressRef.current = 0;
  }, [targetData]);

  const interpolateData = (prev: RenderData, target: RenderData, progress: number): RenderData => {
    const easeProgress = 1 - Math.pow(1 - progress, 3);

    const interpolateLevels = (
      prevLevels: (PriceLevel & { cumulative: number; percent: number })[],
      targetLevels: (PriceLevel & { cumulative: number; percent: number })[]
    ) => {
      const result: (PriceLevel & { cumulative: number; percent: number })[] = [];
      const maxLen = Math.max(prevLevels.length, targetLevels.length);

      for (let i = 0; i < maxLen; i++) {
        const p = prevLevels[i] || prevLevels[prevLevels.length - 1] || { price: 0, quantity: 0, orders: 0, cumulative: 0, percent: 0 };
        const t = targetLevels[i] || targetLevels[targetLevels.length - 1] || p;

        result.push({
          price: t.price,
          quantity: p.quantity + (t.quantity - p.quantity) * easeProgress,
          orders: t.orders,
          cumulative: p.cumulative + (t.cumulative - p.cumulative) * easeProgress,
          percent: p.percent + (t.percent - p.percent) * easeProgress,
        });
      }
      return result;
    };

    return {
      maxBidQty: prev.maxBidQty + (target.maxBidQty - prev.maxBidQty) * easeProgress,
      maxAskQty: prev.maxAskQty + (target.maxAskQty - prev.maxAskQty) * easeProgress,
      maxTotalQty: prev.maxTotalQty + (target.maxTotalQty - prev.maxTotalQty) * easeProgress,
      bidLevels: interpolateLevels(prev.bidLevels, target.bidLevels),
      askLevels: interpolateLevels(prev.askLevels, target.askLevels),
      priceRange: {
        min: prev.priceRange.min + (target.priceRange.min - prev.priceRange.min) * easeProgress,
        max: prev.priceRange.max + (target.priceRange.max - prev.priceRange.max) * easeProgress,
      },
    };
  };

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const formatPrice = (price: number) => {
      return price.toFixed(2);
    };

    const formatQuantity = (qty: number) => {
      if (qty >= 1000000) return (qty / 1000000).toFixed(2) + 'M';
      if (qty >= 1000) return (qty / 1000).toFixed(2) + 'K';
      return qty.toFixed(0);
    };

    const render = (data: RenderData) => {
      ctx.clearRect(0, 0, width, height);

      ctx.fillStyle = '#0d1320';
      ctx.fillRect(0, 0, width, height);

      ctx.strokeStyle = COLORS.grid;
      ctx.lineWidth = 1;
      for (let i = 0; i <= 4; i++) {
        const y = padding.top + (chartHeight / 4) * i;
        ctx.beginPath();
        ctx.moveTo(padding.left, y);
        ctx.lineTo(width - padding.right, y);
        ctx.stroke();
      }

      const priceStep = data.bidLevels.length > 0
        ? (data.bidLevels[0].price - data.bidLevels[data.bidLevels.length - 1].price) / Math.max(data.bidLevels.length - 1, 1)
        : 0.01;

      const rowHeight = chartHeight / Math.max(data.bidLevels.length, data.askLevels.length, 10);

      if (data.bidLevels.length > 0) {
        ctx.fillStyle = COLORS.bidBg;
        ctx.beginPath();
        ctx.moveTo(padding.left, padding.top);

        for (let i = 0; i < data.bidLevels.length; i++) {
          const level = data.bidLevels[i];
          const x = padding.left + level.percent * chartWidth;
          const y = padding.top + i * rowHeight;
          ctx.lineTo(x, y);
        }

        if (data.bidLevels.length > 0) {
          ctx.lineTo(padding.left, padding.top + (data.bidLevels.length - 1) * rowHeight);
        }
        ctx.closePath();
        ctx.fill();

        for (let i = 0; i < data.bidLevels.length; i++) {
          const level = data.bidLevels[i];
          const y = padding.top + i * rowHeight;
          const barWidth = level.percent * chartWidth;

          const gradient = ctx.createLinearGradient(padding.left, y, padding.left + barWidth, y);
          gradient.addColorStop(0, 'rgba(0, 212, 152, 0.4)');
          gradient.addColorStop(1, 'rgba(0, 212, 152, 0.1)');

          ctx.fillStyle = gradient;
          ctx.fillRect(padding.left, y, barWidth, rowHeight - 1);

          ctx.fillStyle = COLORS.textSecondary;
          ctx.font = '11px Monaco, monospace';
          ctx.textAlign = 'right';
          ctx.fillText(formatPrice(level.price), padding.left - 10, y + rowHeight / 2 + 4);

          ctx.fillStyle = COLORS.bid;
          ctx.textAlign = 'left';
          ctx.fillText(formatQuantity(level.quantity), padding.left + 5, y + rowHeight / 2 + 4);

          ctx.fillStyle = COLORS.textSecondary;
          ctx.textAlign = 'right';
          ctx.fillText(formatQuantity(level.cumulative), width - padding.right - 10, y + rowHeight / 2 + 4);
        }
      }

      if (data.askLevels.length > 0) {
        const askStartY = padding.top + data.bidLevels.length * rowHeight + 30;

        ctx.fillStyle = COLORS.askBg;
        ctx.beginPath();
        ctx.moveTo(padding.left, askStartY);

        for (let i = 0; i < data.askLevels.length; i++) {
          const level = data.askLevels[i];
          const x = padding.left + level.percent * chartWidth;
          const y = askStartY + i * rowHeight;
          ctx.lineTo(x, y);
        }

        if (data.askLevels.length > 0) {
          ctx.lineTo(padding.left, askStartY + (data.askLevels.length - 1) * rowHeight);
        }
        ctx.closePath();
        ctx.fill();

        for (let i = 0; i < data.askLevels.length; i++) {
          const level = data.askLevels[i];
          const y = askStartY + i * rowHeight;
          const barWidth = level.percent * chartWidth;

          const gradient = ctx.createLinearGradient(padding.left, y, padding.left + barWidth, y);
          gradient.addColorStop(0, 'rgba(255, 80, 100, 0.4)');
          gradient.addColorStop(1, 'rgba(255, 80, 100, 0.1)');

          ctx.fillStyle = gradient;
          ctx.fillRect(padding.left, y, barWidth, rowHeight - 1);

          ctx.fillStyle = COLORS.textSecondary;
          ctx.font = '11px Monaco, monospace';
          ctx.textAlign = 'right';
          ctx.fillText(formatPrice(level.price), padding.left - 10, y + rowHeight / 2 + 4);

          ctx.fillStyle = COLORS.ask;
          ctx.textAlign = 'left';
          ctx.fillText(formatQuantity(level.quantity), padding.left + 5, y + rowHeight / 2 + 4);

          ctx.fillStyle = COLORS.textSecondary;
          ctx.textAlign = 'right';
          ctx.fillText(formatQuantity(level.cumulative), width - padding.right - 10, y + rowHeight / 2 + 4);
        }
      }

      if (lastPrice) {
        const spreadMidY = padding.top + data.bidLevels.length * rowHeight + 15;

        ctx.fillStyle = 'rgba(13, 19, 32, 0.95)';
        ctx.fillRect(padding.left - 10, spreadMidY - 18, width - padding.left - padding.right + 20, 36);

        ctx.strokeStyle = priceChange >= 0 ? COLORS.lastPriceUp : COLORS.lastPriceDown;
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(padding.left, spreadMidY);
        ctx.lineTo(width - padding.right, spreadMidY);
        ctx.stroke();

        ctx.fillStyle = priceChange >= 0 ? COLORS.lastPriceUp : COLORS.lastPriceDown;
        ctx.font = 'bold 16px Monaco, monospace';
        ctx.textAlign = 'center';
        ctx.fillText(formatPrice(lastPrice), width / 2, spreadMidY + 6);

        ctx.font = '11px Monaco, monospace';
        const changeText = (priceChange >= 0 ? '+' : '') + priceChange.toFixed(2);
        ctx.fillText(changeText, width / 2 + 80, spreadMidY + 6);
      }

      ctx.fillStyle = COLORS.textSecondary;
      ctx.font = '10px Monaco, monospace';
      ctx.textAlign = 'left';
      ctx.fillText('PRICE', 10, padding.top - 15);
      ctx.fillText('SIZE', padding.left + 5, padding.top - 15);
      ctx.textAlign = 'right';
      ctx.fillText('TOTAL', width - padding.right - 10, padding.top - 15);
    };

    const animate = () => {
      const target = targetDataRef.current;
      if (!target) {
        animationRef.current = requestAnimationFrame(animate);
        return;
      }

      if (!prevDataRef.current) {
        prevDataRef.current = target;
      }

      animationProgressRef.current = Math.min(animationProgressRef.current + 0.08, 1);

      const data = animationProgressRef.current < 1
        ? interpolateData(prevDataRef.current, target, animationProgressRef.current)
        : target;

      if (animationProgressRef.current >= 1) {
        prevDataRef.current = target;
      }

      render(data);
      animationRef.current = requestAnimationFrame(animate);
    };

    animationRef.current = requestAnimationFrame(animate);

    return () => {
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }
    };
  }, [width, height, padding.left, padding.right, padding.top, chartWidth, chartHeight, lastPrice, priceChange]);

  return (
    <canvas
      ref={canvasRef}
      width={width}
      height={height}
      style={{
        display: 'block',
        borderRadius: '8px',
        background: '#0d1320',
      }}
    />
  );
}
