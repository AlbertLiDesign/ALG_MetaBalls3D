using System;
using System.Collections.Generic;
using Grasshopper.Kernel;
using Rhino.Geometry;
using Grasshopper.Kernel.Types;
using System.Drawing;
using System.Diagnostics;
using Grasshopper;
using Grasshopper.Kernel.Data;
using System.Linq;
using System.Threading.Tasks;

namespace ALG.MetaBalls3D
{
    public class MetaBalls3D : GH_Component
    {
        public MetaBalls3D()
         : base("MetaBalls3D_Alea", "MetaBalls3D", "Extract isosurface from points using marching cubes algorithm on GPU.", "Mesh", "Triangulation") { }
        public override GH_Exposure Exposure => GH_Exposure.primary;
        protected override Bitmap Icon => null;
        protected override void RegisterInputParams(GH_Component.GH_InputParamManager pManager)
        {
            pManager.AddPointParameter("Point", "P", "Sample points.", GH_ParamAccess.list);
            pManager.AddNumberParameter("Boundary", "B", "The scale of the boundingbox's boundary.", GH_ParamAccess.item, 1.1);
            pManager.AddNumberParameter("VoxelSize", "S", "Voxel Size", GH_ParamAccess.item);
            pManager.AddNumberParameter("Isovalue", "Iso", "Isovalue.", GH_ParamAccess.item);
            pManager.AddNumberParameter("Fusion", "F", "Fusion.", GH_ParamAccess.item, 1.0);
        }

        protected override void RegisterOutputParams(GH_Component.GH_OutputParamManager pManager)
        {
            pManager.AddMeshParameter("Mesh", "M", "Extract isosurface.", GH_ParamAccess.item);
            pManager.AddNumberParameter("Time", "T", "time", GH_ParamAccess.list);
        }
        public static Box CreateUnionBBoxFromGeometry(List<Point3d> pts, double scale)
        {

            Plane worldXY = Plane.WorldXY;
            Transform xform = Transform.ChangeBasis(Plane.WorldXY, worldXY);
            BoundingBox empty = BoundingBox.Empty;
            int num = pts.Count - 1;
            for (int i = 0; i <= num; i++)
            {
                if (pts[i] != null)
                {
                    GH_Point ghp = new GH_Point(pts[i]);
                    BoundingBox boundingBox = ghp.GetBoundingBox(xform);
                    empty.Union(boundingBox);
                }
            }

            Transform xform2 = Transform.Scale(empty.Center, scale);
            empty.Transform(xform2);
            Box box = new Box(empty);
            return box;
        }
        protected override void SolveInstance(IGH_DataAccess DA)
        {
            #region input parameters
            List<Point3d> samplePts = new List<Point3d>();
            double scale = 1.0;
            double boundaryRatio = 2.0;
            double isovalue = 5.0;
            double fusion = 0.0;
            List<double> time = new List<double>();
            Stopwatch sw = new Stopwatch();

            DA.GetDataList("Point", samplePts);
            DA.GetData("Boundary", ref boundaryRatio);
            DA.GetData("VoxelSize", ref scale);
            DA.GetData("Isovalue", ref isovalue);
            DA.GetData("Fusion", ref fusion);
            #endregion

            #region initialization
            Box box1 = CreateUnionBBoxFromGeometry(samplePts, boundaryRatio);

            Interval xD = box1.X;
            Interval yD = box1.Y;
            Interval zD = box1.Z;

            int xCount = (int)Math.Abs(Math.Round(((xD.T1 - xD.T0) / scale), MidpointRounding.AwayFromZero));
            int yCount = (int)Math.Abs(Math.Round(((yD.T1 - yD.T0) / scale), MidpointRounding.AwayFromZero));
            int zCount = (int)Math.Abs(Math.Round(((zD.T1 - zD.T0) / scale), MidpointRounding.AwayFromZero));

            Point3d[] a = box1.GetCorners();
            List<double> b = new List<double>();
            for (int i = 0; i < 8; i++)
            {
                double t = a[i].X + a[i].Y + a[i].Z;
                b.Add(t);
            }
            Point3d baseP = a[b.IndexOf(b.Min())];

            if (fusion < 0)
            {
                AddRuntimeMessage(GH_RuntimeMessageLevel.Error, "The fusion value is too small.");
                return;
            }
            var isoSurface = new Metaballs(baseP, xCount, yCount, zCount, (float)scale, (float)isovalue, (float)fusion + 1.0f, samplePts);
            #endregion

            sw.Start();
            
            isoSurface.runClassifyVoxel();
            isoSurface.runExtractActiveVoxels();
            List<Point3f> resultPts  = isoSurface.runExtractIsoSurfaceGPU();

            this.Message = isoSurface.numVoxels.ToString();
            sw.Stop();
            double tb = sw.Elapsed.TotalMilliseconds;
            // extract the mesh from result vertices

            sw.Restart();
            Mesh mesh = ExtractMesh(resultPts);
            mesh.FaceNormals.ComputeFaceNormals();
            mesh.Normals.ComputeNormals();

            sw.Stop();
            double tc = sw.Elapsed.TotalMilliseconds;

            time.Add(tb);
            time.Add(tc);

            DA.SetData("Mesh", mesh);
            DA.SetDataList("Time", time);

        }
        public static Mesh ExtractMesh(List<Point3f> pts)
        {
            Mesh mesh = new Mesh();
            int FCount = pts.Count / 3;

            mesh.Vertices.AddVertices(pts);

            MeshFace[] mfs = new MeshFace[FCount];
            Parallel.For(0, FCount, i =>
            {
                MeshFace mf = new MeshFace(i * 3, i * 3 + 1, i * 3 + 2);
                mfs[i] = mf;
            });
            mesh.Faces.AddFaces(mfs);
            return mesh;
        }

        public override Guid ComponentGuid
        {
            get { return new Guid("{F6603844-40DF-43C9-8A24-BAC7A58A719D}"); }
        }
    }
}
