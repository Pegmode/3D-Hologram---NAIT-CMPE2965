using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.IO.Hashing;
using System.Linq;
using System.Reflection.PortableExecutable;
using System.Text;
using System.Threading.Tasks;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;

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
        public const int CURRENT_VERSION = 2;
        public const UInt32 Magic = 0x484F4C4FU; //HOLO
        public byte Version { get; set; }
        public byte HeaderSizeBytes { get { return 25; } }
        public sbyte DataType { get; set; }
        public Int32 FrameCount { get; set; }
        public Int32 SliceCount { get; set; }
        public Int32 PayloadBytes { get; set; }
        public Int16 MotorSpeedRpm { get; set; }
        public UInt32 PayloadCrc32 { get; set; }

        public MessageHeader() {// lagacy empty constructor

        }

        public MessageHeader(WifiTxDataType dataType, Int32 frameCount, Int32 sliceCount, Int32 payloadBytes, Int16 motorSpeed, UInt32 payloadCrc32) {
            this.DataType = (sbyte)dataType;
            this.FrameCount = frameCount;
            this.SliceCount = sliceCount;
            this.PayloadBytes = payloadBytes;
            this.MotorSpeedRpm = motorSpeed;
            this.PayloadCrc32 = 0;//Header does not contain the payload so dont fill the CRC in the header CTR
            this.Version = CURRENT_VERSION;//this ctr does not contain the override for version
        }

        public MessageHeader(WifiTxDataType dataType) {//Use this CTR for message passing
            if (dataType == WifiTxDataType.Still3D || dataType == WifiTxDataType.Animation3D) {
                throw new Exception("Error: Can't create a message header of type Still3D or Animation3D without payload info or payload data");
            }
            this.DataType = (sbyte)dataType;
        }

        public MessageHeader(Int16 motorSpeed) : this(WifiTxDataType.None) {//use this ctr for changing motor speed
            this.MotorSpeedRpm = motorSpeed;
        }

        public static byte[] Build3DImageMessage(byte[] payload, Int32 frameCount, Int32 sliceCount, WifiTxDataType messageType, Int16 rpm) {
            Int32 payloadBytes = payload.Length;
            Crc32.HashToUInt32(payload);
            MessageHeader messageHeader = new MessageHeader(
                WifiTxDataType.Still3D,
                frameCount,
                sliceCount,
                payloadBytes,
                rpm,
                Crc32.HashToUInt32(payload)
            );
            //pack the message 
            byte[] header = messageHeader.GetBytes();
            byte[] txBytes = new byte[header.Length + payload.Length];
            Buffer.BlockCopy(header, 0, txBytes, 0, header.Length);
            Buffer.BlockCopy(payload, 0, txBytes, header.Length, payload.Length);
            return txBytes;
        }

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
