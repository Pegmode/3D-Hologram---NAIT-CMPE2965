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

    public enum WifiTxDataType
    {
        None = -1,
        Invalid = 0,
        Still3D = 1,
        Animation3D = 2
    }

    public enum WifiTxMotor
    {
        Off = -1,
        Same = 0
    }

    public class MessageHeader
    {
        public const UInt32 Magic = 0x484F4C4FU; //HOLO
        public byte Version { get; set; }
        public byte HeaderSizeBytes { get { return 25; } }
        public sbyte DataType { get; set; }
        public Int32 FrameCount { get; set; }
        public Int32 SliceCount { get; set; }
        public Int32 PayloadBytes { get; set; }
        public Int16 MotorSpeedRpm { get; set; }
        public UInt32 PayloadCrc32 { get; set; }

        public byte[] GetBytes()
        {
            byte[] bytes = new byte[HeaderSizeBytes];
            int offset = 0;

            WriteUInt32BE(bytes, ref offset, Magic);
            WriteByte(bytes, ref offset, Version);
            WriteByte(bytes, ref offset, HeaderSizeBytes);
            WriteSByte(bytes, ref offset, DataType);
            WriteInt32BE(bytes, ref offset, FrameCount);
            WriteInt32BE(bytes, ref offset, SliceCount);
            WriteInt32BE(bytes, ref offset, PayloadBytes);
            WriteInt16BE(bytes, ref offset, MotorSpeedRpm);
            WriteUInt32BE(bytes, ref offset, PayloadCrc32);

            return bytes;
        }

        private static void WriteByte(byte[] buffer, ref int offset, byte value)
        {
            buffer[offset] = value;
            offset += 1;
        }

        private static void WriteSByte(byte[] buffer, ref int offset, sbyte value)
        {
            buffer[offset] = unchecked((byte)value);
            offset += 1;
        }

        private static void WriteInt16BE(byte[] buffer, ref int offset, short value)
        {
            BinaryPrimitives.WriteInt16BigEndian(buffer.AsSpan(offset, 2), value);
            offset += 2;
        }

        private static void WriteInt32BE(byte[] buffer, ref int offset, int value)
        {
            BinaryPrimitives.WriteInt32BigEndian(buffer.AsSpan(offset, 4), value);
            offset += 4;
        }

        private static void WriteUInt32BE(byte[] buffer, ref int offset, uint value)
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
                $"PayloadCrc32: 0x{PayloadCrc32:X8}";
        }

    }
}
