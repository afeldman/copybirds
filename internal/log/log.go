// Package log wraps zap + lumberjack für copybirds.
// Console: farbiger, lesbarer Output. File (optional): JSON mit Rotation.
package log

import (
	"os"

	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
	"gopkg.in/natefinch/lumberjack.v2"
)

var sugar *zap.SugaredLogger

func init() {
	// Sicherer Default bis Init() aufgerufen wird.
	encCfg := zap.NewDevelopmentEncoderConfig()
	encCfg.EncodeLevel = zapcore.CapitalColorLevelEncoder
	core := zapcore.NewCore(
		zapcore.NewConsoleEncoder(encCfg),
		zapcore.AddSync(os.Stderr),
		zapcore.WarnLevel,
	)
	sugar = zap.New(core).Sugar()
}

// Init konfiguriert den Logger. Wird von Cobra PersistentPreRun aufgerufen.
//
//   verbosity <= 0 → Error
//   verbosity  1   → Warn  (default)
//   verbosity  2   → Info
//   verbosity >= 3 → Debug
//
// logFile: wenn gesetzt, werden Logs zusätzlich als JSON in die Datei geschrieben
// (automatische Rotation via lumberjack).
func Init(verbosity int, logFile string) {
	encCfg := zap.NewDevelopmentEncoderConfig()
	encCfg.EncodeLevel = zapcore.CapitalColorLevelEncoder
	encCfg.EncodeTime = zapcore.TimeEncoderOfLayout("15:04:05")

	consoleCore := zapcore.NewCore(
		zapcore.NewConsoleEncoder(encCfg),
		zapcore.AddSync(os.Stderr),
		verbosityToLevel(verbosity),
	)

	cores := []zapcore.Core{consoleCore}

	if logFile != "" {
		rotator := &lumberjack.Logger{
			Filename:   logFile,
			MaxSize:    10,  // MB
			MaxBackups: 5,
			MaxAge:     30,  // Tage
			Compress:   true,
		}
		fileCore := zapcore.NewCore(
			zapcore.NewJSONEncoder(zap.NewProductionEncoderConfig()),
			zapcore.AddSync(rotator),
			zapcore.DebugLevel,
		)
		cores = append(cores, fileCore)
	}

	logger := zap.New(
		zapcore.NewTee(cores...),
		zap.AddCaller(),
		zap.AddCallerSkip(1),
	)
	sugar = logger.Sugar()
}

func verbosityToLevel(v int) zapcore.Level {
	switch {
	case v <= 0:
		return zapcore.ErrorLevel
	case v == 1:
		return zapcore.WarnLevel
	case v == 2:
		return zapcore.InfoLevel
	default:
		return zapcore.DebugLevel
	}
}

// Sync flusht gepufferte Log-Einträge. Vor Programmende aufrufen.
func Sync() { _ = sugar.Desugar().Sync() }

// With gibt einen Logger mit strukturierten Feldern zurück.
func With(args ...any) *zap.SugaredLogger { return sugar.With(args...) }

func Debug(args ...any)            { sugar.Debug(args...) }
func Info(args ...any)             { sugar.Info(args...) }
func Warn(args ...any)             { sugar.Warn(args...) }
func Error(args ...any)            { sugar.Error(args...) }
func Fatal(args ...any)            { sugar.Fatal(args...) }
func Debugf(f string, a ...any)    { sugar.Debugf(f, a...) }
func Infof(f string, a ...any)     { sugar.Infof(f, a...) }
func Warnf(f string, a ...any)     { sugar.Warnf(f, a...) }
func Errorf(f string, a ...any)    { sugar.Errorf(f, a...) }
func Fatalf(f string, a ...any)    { sugar.Fatalf(f, a...) }
