// Hutaomu Editor (Rust) - 程序化绘制的矢量图标（无位图依赖，随主题换色）。
use egui::{Color32, Painter, Pos2, Rect, Stroke, Vec2};

#[derive(Clone, Copy, PartialEq)]
pub enum Icon {
    Explorer,
    Search,
    Outline,
    Plugins,
    Info,
    Plus,
    Close,
    Minimize,
    Maximize,
    Restore,
    Folder,
    File,
    ArrowUp,
    ArrowDown,
}

/// 在 rect 内居中绘制图标（16 视口设计，线宽 1.4，round cap）
pub fn draw(painter: &Painter, rect: Rect, icon: Icon, color: Color32) {
    let c = rect.center();
    let s = rect.width().min(rect.height());
    let u = s / 16.0;
    let stroke = Stroke::new(1.4, color);
    let p = |x: f32, y: f32| Pos2::new(c.x + (x - 8.0) * u, c.y + (y - 8.0) * u);

    match icon {
        Icon::Explorer | Icon::Folder => {
            let pts = vec![
                p(2.5, 12.5),
                p(2.5, 4.5),
                p(6.5, 4.5),
                p(8.0, 6.0),
                p(13.5, 6.0),
                p(13.5, 12.5),
            ];
            painter.add(egui::Shape::Path(egui::epaint::PathShape::closed_line(pts, stroke)));
        }
        Icon::Search => {
            painter.circle_stroke(p(7.0, 7.0), 4.2 * u, stroke);
            painter.line_segment([p(10.2, 10.2), p(13.4, 13.4)], stroke);
        }
        Icon::Outline => {
            for (y, len) in [(4.0, 9.0), (8.0, 6.5), (12.0, 8.0)] {
                painter.line_segment([p(4.5, y), p(4.5 + len, y)], stroke);
                painter.circle_filled(p(2.5, y), 0.9 * u, color);
            }
        }
        Icon::Plugins => {
            painter.rect_filled(
                Rect::from_min_size(p(2.5, 2.5), Vec2::splat(5.0 * u)),
                1.2 * u,
                color,
            );
            for (x, y) in [(8.9, 2.5), (2.5, 8.9), (8.9, 8.9)] {
                painter.rect_stroke(
                    Rect::from_min_size(p(x, y), Vec2::splat(5.0 * u)),
                    1.2 * u,
                    stroke,
                    egui::StrokeKind::Inside,
                );
            }
        }
        Icon::Info => {
            painter.circle_stroke(c, 6.2 * u, stroke);
            painter.circle_filled(p(8.0, 4.6), 0.9 * u, color);
            painter.line_segment([p(8.0, 7.2), p(8.0, 11.4)], Stroke::new(1.6, color));
        }
        Icon::Plus => {
            painter.line_segment([p(8.0, 3.5), p(8.0, 12.5)], stroke);
            painter.line_segment([p(3.5, 8.0), p(12.5, 8.0)], stroke);
        }
        Icon::Close => {
            painter.line_segment([p(4.5, 4.5), p(11.5, 11.5)], stroke);
            painter.line_segment([p(11.5, 4.5), p(4.5, 11.5)], stroke);
        }
        Icon::Minimize => {
            painter.line_segment([p(4.0, 8.0), p(12.0, 8.0)], stroke);
        }
        Icon::Maximize => {
            painter.rect_stroke(
                Rect::from_min_size(p(4.0, 4.0), Vec2::splat(8.0 * u)),
                1.0 * u,
                stroke,
                egui::StrokeKind::Inside,
            );
        }
        Icon::Restore => {
            painter.rect_stroke(
                Rect::from_min_size(p(5.5, 2.8), Vec2::splat(7.0 * u)),
                1.0 * u,
                stroke,
                egui::StrokeKind::Inside,
            );
            painter.rect_stroke(
                Rect::from_min_size(p(3.2, 5.8), Vec2::splat(7.0 * u)),
                1.0 * u,
                stroke,
                egui::StrokeKind::Inside,
            );
        }
        Icon::File => {
            let pts = vec![
                p(4.0, 2.5),
                p(9.5, 2.5),
                p(12.0, 5.0),
                p(12.0, 13.5),
                p(4.0, 13.5),
            ];
            painter.add(egui::Shape::Path(egui::epaint::PathShape::closed_line(pts, stroke)));
            painter.line_segment([p(6.0, 8.0), p(10.0, 8.0)], stroke);
            painter.line_segment([p(6.0, 10.5), p(10.0, 10.5)], stroke);
        }
        Icon::ArrowUp => {
            painter.line_segment([p(8.0, 4.0), p(8.0, 12.0)], stroke);
            painter.line_segment([p(4.5, 7.5), p(8.0, 4.0)], stroke);
            painter.line_segment([p(11.5, 7.5), p(8.0, 4.0)], stroke);
        }
        Icon::ArrowDown => {
            painter.line_segment([p(8.0, 4.0), p(8.0, 12.0)], stroke);
            painter.line_segment([p(4.5, 8.5), p(8.0, 12.0)], stroke);
            painter.line_segment([p(11.5, 8.5), p(8.0, 12.0)], stroke);
        }
    }
}
