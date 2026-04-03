using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Reflection.PortableExecutable;
using System.Text;
using System.Threading.Tasks;

namespace TestUIProject
{
    public class MessageHeader
    {
        public const UInt32 Magic = 0x484F4C4FU; //HOLO
        public UInt16 Version { get; set; }
        public UInt16 HeaderSizeBytes { get { return 36; } }
        public UInt32 DataType { get; set; }
        public UInt32 FrameCount { get; set; }
        public UInt32 SliceCount { get; set; }
        public UInt32 PayloadBytes { get; set; }
        public UInt32 MotorSpeedRpm { get; set; }
        public UInt32 Flags { get; set; }
        public UInt32 PayloadCrc32 { get; set; }

        public byte[] GetBytes()
        {
            byte[] bytes = new byte[HeaderSizeBytes];
            int offset = 0;

            WriteUInt32BE(bytes, ref offset, Magic);
            WriteUInt16BE(bytes, ref offset, Version);
            WriteUInt16BE(bytes, ref offset, HeaderSizeBytes);
            WriteUInt32BE(bytes, ref offset, DataType);
            WriteUInt32BE(bytes, ref offset, FrameCount);
            WriteUInt32BE(bytes, ref offset, SliceCount);
            WriteUInt32BE(bytes, ref offset, PayloadBytes);
            WriteUInt32BE(bytes, ref offset, MotorSpeedRpm);
            WriteUInt32BE(bytes, ref offset, Flags);
            WriteUInt32BE(bytes, ref offset, PayloadCrc32);

            return bytes;
        }

        private static void WriteUInt16BE(byte[] buffer, ref int offset, UInt16 value)
        {
            BinaryPrimitives.WriteUInt16BigEndian(buffer.AsSpan(offset, 2), value);
            offset += 2;
        }

        private static void WriteUInt32BE(byte[] buffer, ref int offset, UInt32 value)
        {
            BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(offset, 4), value);
            offset += 4;
        }

        public override string ToString()
        {
            return
                $"Magic: 0x{Magic:X8}, " +
                $"Version: {Version}, " +
                $"HeaderSizeBytes: {HeaderSizeBytes}, " +
                $"DataType: {DataType}, " +
                $"FrameCount: {FrameCount}, " +
                $"SliceCount: {SliceCount}, " +
                $"PayloadBytes: {PayloadBytes}, " +
                $"MotorSpeedRpm: {MotorSpeedRpm}, " +
                $"Flags: {Flags}, " +
                $"PayloadCrc32: 0x{PayloadCrc32:X8}";
        }

    }
}
