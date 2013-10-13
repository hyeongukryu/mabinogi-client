using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Reverse
{
    class Program
    {
        static void Main(string[] args)
        {
            var file_a = Console.ReadLine();
            var file_b = Console.ReadLine();

            var content = File.ReadAllBytes(file_a);
            File.WriteAllBytes(file_b, content.Reverse().ToArray());
        }
    }
}
